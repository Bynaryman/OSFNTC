from dataclasses import dataclass, field
from enum import Enum
from typing import (
    Dict,
    List,
    Optional,
    Union
)

import threading
import torch
import torch.distributed as dist
from torch.distributed import rpc
from torch.distributed import distributed_c10d
from torch.distributed._sharding_spec import (
    ChunkShardingSpec,
    EnumerableShardingSpec,
    ShardMetadata,
    ShardingSpec,
)
from torch.distributed._sharding_spec._internals import (
    check_tensor,
    get_split_size,
    get_chunked_dim_size,
    validate_non_overlapping_shards_metadata,
)
from torch.types import Number
from .metadata import TensorProperties, ShardedTensorMetadata
from .ops import (
    kaiming_uniform_,
    normal_,
    sharded_embedding,
    sharded_embedding_bag,
    sharded_linear,
    uniform_,
)
from .shard import Shard
from .utils import (
    get_current_process_group,
    _flatten_tensor_size,
    _parse_and_validate_remote_device,
    _validate_output_tensor_for_gather,
    build_metadata_from_local_shards,
    build_global_metadata
)

# Tracking for sharded tensor objects.
_sharded_tensor_lock = threading.Lock()
_sharded_tensor_current_id = 0
_sharded_tensor_map: Dict[int, 'ShardedTensor'] = {}

def _register_remote_shards(sharded_tensor_id: int, rrefs: List[rpc.RRef[Shard]], rpc_rank: int):
    with _sharded_tensor_lock:
        if sharded_tensor_id not in _sharded_tensor_map:
            raise RuntimeError(
                f'Could not find sharded_tensor_id: {sharded_tensor_id} in map: {_sharded_tensor_map.keys()}')

        _sharded_tensor_map[sharded_tensor_id]._register_remote_shards(rrefs, rpc_rank)


class CreateOp(Enum):
    EMPTY = 0
    FULL = 1
    ONES = 2
    RAND = 3
    ZEROS = 4


@dataclass
class TensorInitParams(object):
    """ Container for list of common params to create new local tensor. """

    create_op: CreateOp

    # needed when create_op is FULL
    # default set to False (not None) since None is incompatible with Number.
    fill_value: Number = field(default=False)

    tensor_properties: TensorProperties = field(
        default=TensorProperties(dtype=torch.get_default_dtype(),
                                 layout=torch.strided,
                                 requires_grad=False,
                                 memory_format=torch.contiguous_format,
                                 pin_memory=False))



class ShardedTensor(object):
    """
    ShardedTensor is an abstraction to represent Tensors that are sharded
    across multiple devices and multiple processes.

    ShardedTensor is initialized in an SPMD like fashion where each rank
    initializes the ShardedTensor. The ShardedTensor object on each rank
    then only stores the local shard for the Tensor and provides global
    metadata for all the shards.

    ShardedTensor doesn't provide any Tensor like operations but is a wrapper
    providing the Tensor representing the local shard and the global metadata.
    Using these, users can build their custom distributed sharded computations
    on top of this primitive. The local shards are all initialized using the
    create_op specified by tensor_init_params.create_op, e.g., torch.ones, or
    torch.empty

    Args:
        sharding_spec (:class:`torch.distributed._sharding_spec.ShardingSpec`): The specification
            describing how to shard the Tensor.
        size (int...): a sequence of integers defining the shape of the output
            tensor. Can be a variable number of arguments or a collection like a list or tuple.

    Keyword args:
        tensor_init_params (:class: `TensorInitParams`): common params to create tensor.
        init_rrefs (bool, optional): Whether or not to initialize
            :class:`torch.distributed.rpc.RRef`s pointing to remote shards.
            Need to initialize the RPC Framework if specified as ``True``.
            Default: ``False``.
    """

    def __new__(cls, *args, **kwargs):
        # Use __new__ for logging purposes.
        torch._C._log_api_usage_once("torch.distributed.sharded_tensor")
        return super(ShardedTensor, cls).__new__(cls)

    def __init__(
        self,
        sharding_spec: ShardingSpec,
        *size,
        tensor_init_params: TensorInitParams,
        process_group=None,
        init_rrefs=False,
    ):
        # prepare initialization, initialize fields like
        # _process_group, _local_shards, etc.
        self._prepare_init(process_group=process_group, init_rrefs=init_rrefs)

        if tensor_init_params.tensor_properties is None:
            raise ValueError('tensor_properties must not be None.')

        if tensor_init_params.tensor_properties.dtype is None:
            tensor_init_params.tensor_properties.dtype = torch.get_default_dtype()

        if tensor_init_params.tensor_properties.layout != torch.strided:
            raise ValueError('Only torch.strided layout is currently supported')

        if tensor_init_params.tensor_properties.memory_format != torch.contiguous_format:
            raise ValueError('Only torch.contiguous_format memory_format is currently supported')

        dims = _flatten_tensor_size(size)

        self._sharding_spec = sharding_spec

        if isinstance(self._sharding_spec, ChunkShardingSpec):
            self._init_chunked(dims, tensor_init_params)
        elif isinstance(self._sharding_spec, EnumerableShardingSpec):
            self._init_enumerable(dims, tensor_init_params)
        else:
            raise ValueError(f'Unsupported sharding_spec: {self._sharding_spec}')

        # do post initialization (i.e. register sharded_tensor_id, initialize_rpc)
        self._post_init()

    def _prepare_init(self, process_group=None, init_rrefs=False):
        self._init_rrefs = init_rrefs
        self._sharded_tensor_id = None

        self._process_group = (
            process_group
            if process_group is not None
            else distributed_c10d._get_default_group()
        )

        self._local_shards: List[Shard] = []
        self._remote_shards: Dict[int, List[rpc.RRef[Shard]]] = {}

    def _post_init(self):
        with _sharded_tensor_lock:
            global _sharded_tensor_current_id, _sharded_tensor_map
            self._sharded_tensor_id = _sharded_tensor_current_id
            _sharded_tensor_map[self._sharded_tensor_id] = self
            _sharded_tensor_current_id += 1

        # Initialize RPC if available.
        if self._init_rrefs:
            if not rpc._is_current_rpc_agent_set():
                raise RuntimeError(
                    'RPC Framework needs to be initialized using'
                    ' torch.distributed.rpc.init_rpc if init_rrefs is set to True')
            self._init_rpc()

    def __del__(self):
        # Clean up the global map.
        with _sharded_tensor_lock:
            global _sharded_tensor_current_id, _sharded_tensor_map
            if self._sharded_tensor_id in _sharded_tensor_map:
                _sharded_tensor_map.pop(self._sharded_tensor_id)  # type: ignore[call-overload]

    def _init_rpc(self):
        # Validate PG and RPC ranks match.
        pg_rank = dist.get_rank()
        rpc_rank = rpc.get_worker_info().id
        if pg_rank != rpc_rank:
            raise ValueError(
                f'Default ProcessGroup and RPC ranks must be '
                f'the same for ShardedTensor, found process group rank: '
                f'{pg_rank} and RPC rank: {rpc_rank}'
            )

        self._remote_shards = {}

        # Gather all the sharded tensor ids.
        world_size = dist.get_world_size(self._process_group)
        worker_infos = rpc._get_current_rpc_agent().get_worker_infos()
        rank_to_name = {}
        name_to_rank = {}

        for worker_info in worker_infos:
            rank_to_name[worker_info.id] = worker_info.name
            name_to_rank[worker_info.name] = worker_info.id

        all_tensor_ids = rpc.api._all_gather(self._sharded_tensor_id)

        # Share the local shards to the entire world.
        futs = []
        rpc_rank = rpc.get_worker_info().id
        for rank in range(dist.get_world_size()):
            # Skip self.
            if rank == dist.get_rank():
                continue

            if len(self.local_shards()) != 0:
                rrefs: List[rpc.RRef[Shard]] = [rpc.RRef(shard) for shard in self.local_shards()]
                fut = rpc.rpc_async(
                    rank,
                    _register_remote_shards,
                    args=(all_tensor_ids[rank_to_name[rank]], rrefs, rpc_rank))
                futs.append(fut)

        torch.futures.wait_all(futs)

        # Barrier for all RPCs to finish on all ranks.
        rpc.api._all_gather(None)

    def gather(
        self,
        dst: int = 0,
        out: Optional[torch.Tensor] = None,
    ) -> None:
        """
        Creates a full :class:`Tensor` on rank ``dst`` by gathering all shards of the
        sharded tensor.

        The API needs to be called on all ranks in SPMD fashion. All ranks should have
        the same ``dst``. ``out`` should be a tensor of the same size as the overall
        size of the sharded tensor on ``dst`` and ``None`` on all other ranks.

        Args:
            dst(int): The rank where full tensor is constructed.
                Default: 0
            out (:class `torch.Tensor`, optional): The output full tensor.
                Must to be provided ONLY on ``dst`` rank.
                Default: ``None``
        """
        rank = dist.get_rank(self._process_group)
        full_size = self.metadata().size
        _validate_output_tensor_for_gather(rank, dst, full_size, out)

        local_shards = self.local_shards()

        world_size = dist.get_world_size(self._process_group)

        gathered_shards = [None] * world_size
        # will revise this part with CPU support and use dist.gather()
        # once NCCL support for gather() is ready
        # https://github.com/pytorch/pytorch/issues/66187
        device = torch.device(f"cuda:{rank % world_size}")
        with torch.cuda.device(device):
            dist.all_gather_object(
                obj=local_shards,
                object_list=gathered_shards,
                group=self._process_group,
            )

        if rank == dst:
            dims = len(full_size)
            for shards in gathered_shards:
                if shards is None:
                    raise RuntimeError(
                        'Gathered shards cannot be None on dst rank {dst}'
                    )
                for shard in shards:
                    metadata = shard.metadata
                    tensor = shard.tensor

                    out_narrow_view = out
                    for dim in range(dims):
                        out_narrow_view = out_narrow_view.narrow(
                            dim,
                            metadata.shard_offsets[dim],
                            metadata.shard_sizes[dim],
                        )

                    out_narrow_view.copy_(tensor)

    @classmethod
    def _init_from_local_shards(
        cls,
        local_shards: List[Shard],
        *global_size,
        process_group=None,
        init_rrefs=False,
    ):
        # STEP 1: Validate the Shardmetadatas locally
        process_group = (
            process_group
            if process_group is not None
            else distributed_c10d._get_default_group()
        )
        current_rank = dist.get_rank(process_group)
        world_size = dist.get_world_size(process_group)

        local_sharded_tensor_metadata: Optional[ShardedTensorMetadata] = None
        local_shards_device = torch.device("cpu")
        global_tensor_size = _flatten_tensor_size(global_size)

        if len(local_shards) > 0:
            local_sharded_tensor_metadata, local_shards_device = \
                build_metadata_from_local_shards(local_shards, global_tensor_size, current_rank, process_group)

        # STEP 2. Validate metadata across ranks, and build a global sharded tensor
        # metadata by gathering local ShardedTensorMetadata
        gathered_metadatas: List[Optional[ShardedTensorMetadata]] = []
        if world_size > 1:
            gathered_metadatas = [None for _ in range(world_size)]

            if local_shards_device.type == "cuda":
                # with GPU/NCCL, we need to set a device for all_gather_object
                # to use as we need to know which device we should put the
                # serialized tensor on before the NCCL collective.
                with torch.cuda.device(local_shards_device):
                    dist.all_gather_object(
                        gathered_metadatas,
                        local_sharded_tensor_metadata,
                        group=process_group
                    )
            else:
                dist.all_gather_object(
                    gathered_metadatas,
                    local_sharded_tensor_metadata,
                    group=process_group
                )
        else:
            gathered_metadatas = [local_sharded_tensor_metadata]

        global_sharded_tensor_metadata = build_global_metadata(gathered_metadatas)

        # STEP 3: Validation done, create the actual ShardedTensor and populate fields
        # prepare initialization
        sharded_tensor = cls.__new__(cls)
        sharded_tensor._prepare_init(process_group=process_group, init_rrefs=init_rrefs)

        # add to metadata and local_shards
        sharded_tensor._metadata = global_sharded_tensor_metadata
        sharded_tensor._local_shards = local_shards
        # make a EnumerableShardingSpec for sharded tensors that initialized from this API.
        # TODO: make sharding spec a ChunkShardingSpec by inferring from the metadata list.
        #       see issue https://github.com/pytorch/pytorch/issues/67244
        sharded_tensor._sharding_spec = EnumerableShardingSpec(global_sharded_tensor_metadata.shards_metadata)

        # run post initialization, i.e. map registration, rpc initialization
        sharded_tensor._post_init()
        return sharded_tensor

    @classmethod
    def _init_from_local_shards_and_global_metadata(
        cls,
        local_shards: List[Shard],
        sharded_tensor_metadata: ShardedTensorMetadata,
        process_group=None,
        init_rrefs=False,
    ):
        """
        Initialize a ShardedTensor with local shards and a global
        ShardedTensorMetadata built on each rank.

        Warning: This API is experimental and subject to change. It does
                 not do cross rank validations, and fully rely on the user
                 for the correctness of sharded_tensor_metadata on each rank
        """
        process_group = (
            process_group
            if process_group is not None
            else distributed_c10d._get_default_group()
        )
        current_rank = dist.get_rank(process_group)

        shards_metadata = sharded_tensor_metadata.shards_metadata
        tensor_properties = sharded_tensor_metadata.tensor_properties

        if len(shards_metadata) == 0:
            raise ValueError("shards_metadata must not be empty!")

        if tensor_properties.layout != torch.strided:
            raise ValueError('Only torch.strided layout is currently supported')

        sharded_tensor = cls.__new__(cls)
        sharded_tensor._prepare_init(process_group=process_group, init_rrefs=init_rrefs)

        sharded_tensor._metadata = sharded_tensor_metadata

        local_shard_metadatas = []

        def _raise_if_mismatch(expected, actual, prop_name, rank, is_property=False):
            tensor_property_or_metadata = "tensor property" if is_property else "local ShardMetadata"
            if expected != actual:
                raise ValueError(f"Local shards' tensor {prop_name} property is incompatible with "
                                 f"{tensor_property_or_metadata} on rank {rank}: "
                                 f"{tensor_property_or_metadata} {prop_name}={expected}, "
                                 f"local shard tensor {prop_name}={actual}.")

        # collect local shard metadatas from the global sharded_tensor_metadata
        for shard_metadata in shards_metadata:  # type: ignore[attr-defined]
            rank, local_device = _parse_and_validate_remote_device(sharded_tensor._process_group, shard_metadata.placement)

            if current_rank == rank:
                local_shard_metadatas.append(shard_metadata)

        if len(local_shards) != len(local_shard_metadatas):
            raise RuntimeError(
                f'Number of local shards ({len(local_shards)}) does not match number of local '
                f'shards metadata in sharded_tensor_metadata ({len(local_shard_metadatas)}) '
                f'on rank ({current_rank}) '
            )

        for shard in local_shards:
            shard_meta = shard.metadata
            local_shard_tensor = shard.tensor
            rank, local_device = _parse_and_validate_remote_device(sharded_tensor._process_group, shard_meta.placement)

            # validate if shard_meta in the metadatas collected from sharded_tensor_metadata
            assert shard_meta in local_shard_metadatas, \
                "local shard metadata not in sharded_tensor_metadata!"

            if not local_shard_tensor.is_contiguous():
                raise ValueError('Only torch.contiguous_format memory_format is currently supported')

            _raise_if_mismatch(tensor_properties.layout, local_shard_tensor.layout, "layout", current_rank, True)
            _raise_if_mismatch(shard_meta.shard_sizes, list(local_shard_tensor.size()), "size", current_rank)
            _raise_if_mismatch(tensor_properties.pin_memory, local_shard_tensor.is_pinned(), "pin_memory", current_rank, True)
            _raise_if_mismatch(local_device, local_shard_tensor.device, "device", current_rank)
            _raise_if_mismatch(tensor_properties.dtype, local_shard_tensor.dtype, "dtype", current_rank, True)
            _raise_if_mismatch(
                tensor_properties.requires_grad, local_shard_tensor.requires_grad, "requires_grad", current_rank, True)

        # check if shards_metadata have overlap shards
        validate_non_overlapping_shards_metadata(shards_metadata)

        # check if the shards_metadata is compatible with overall size of the sharded tensor.
        check_tensor(shards_metadata, list(sharded_tensor_metadata.size))

        # done validation, add local_shards
        sharded_tensor._local_shards = local_shards
        # make a EnumerableShardingSpec for sharded tensors that initialized from this API.
        # TODO: make sharding spec a ChunkShardingSpec by inferring from the metadata list.
        #       see issue https://github.com/pytorch/pytorch/issues/67244
        sharded_tensor._sharding_spec = EnumerableShardingSpec(shards_metadata)

        # run post initialization, i.e. map registration, rpc initialization
        sharded_tensor._post_init()
        return sharded_tensor


    def _init_chunked(self, dims, tensor_init_params: TensorInitParams, ):
        current_rank = dist.get_rank(self._process_group)
        sharding_dim = self._sharding_spec.dim  # type: ignore[attr-defined]

        # Validate the sharding spec.
        if not isinstance(sharding_dim, int):
            raise ValueError(
                f"Sharding dim needs to be an integer, found: {sharding_dim}"
            )
        if sharding_dim >= len(dims) or sharding_dim < -len(dims):
            raise ValueError(f"Invalid sharding dim: {sharding_dim}")

        dim_size = dims[sharding_dim]
        remote_devices = self._sharding_spec.placements  # type: ignore[attr-defined]
        chunks = len(remote_devices)
        # split_size computed similar to 'torch.chunk'
        split_size = get_split_size(dim_size, chunks)

        shards_metadata = []
        for idx, remote_device in enumerate(remote_devices):
            rank, local_device = _parse_and_validate_remote_device(self._process_group, remote_device)

            # Adjust the sharding dim for this rank.
            sharded_dim_size = get_chunked_dim_size(dim_size, split_size, idx)

            if sharded_dim_size > 0:
                # Build sharding_metadata.

                # deepcopy for modification.
                rank_dims = dims.copy()

                rank_offsets = [0] * len(dims)
                rank_offsets[sharding_dim] = split_size * idx
                rank_dims[sharding_dim] = sharded_dim_size

                shard_metadata = ShardMetadata(rank_offsets, rank_dims, remote_device)
                shards_metadata.append(shard_metadata)

                # Build the local shard for the current rank if it is involved in the sharding spec.
                if current_rank == rank:
                    # Initialize the local shard.
                    local_shard = _create_tensor_from_params(
                        *rank_dims, local_device=local_device, tensor_init_params=tensor_init_params)
                    self._local_shards.append(Shard(local_shard, shard_metadata))

        # Build overall metadata
        self._metadata = ShardedTensorMetadata(
            shards_metadata, dims, tensor_init_params.tensor_properties, )

    def _init_enumerable(self, dims, tensor_init_params: TensorInitParams):
        # Validate the sharding spec is compatible with the tensor.
        check_tensor(self._sharding_spec.shards, dims)  # type: ignore[attr-defined]

        current_rank = dist.get_rank(self._process_group)

        shards_metadata = []
        for shard_metadata in self._sharding_spec.shards:  # type: ignore[attr-defined]
            rank, local_device = _parse_and_validate_remote_device(self._process_group, shard_metadata.placement)
            shards_metadata.append(shard_metadata)

            if current_rank == rank:
                # Initialize the local shard.
                local_shard = _create_tensor_from_params(
                    *shard_metadata.shard_sizes, local_device=local_device,
                    tensor_init_params=tensor_init_params)
                self._local_shards.append(Shard(local_shard, shard_metadata))

        # Build overall metadata
        self._metadata = ShardedTensorMetadata(
            shards_metadata, dims, tensor_init_params.tensor_properties, )

    def sharding_spec(self) -> ShardingSpec:
        """
        Returns the ShardingSpec for the tensor.
        """
        return self._sharding_spec

    def __torch_function__(self, func, types, args=(), kwargs=None):
        if func == torch.nn.functional.linear:
            return sharded_linear(types, args, kwargs, self._process_group)
        if func == torch.nn.functional.embedding:
            return sharded_embedding(types, args, kwargs, self._process_group)
        if func == torch.nn.functional.embedding_bag:
            return sharded_embedding_bag(types, args, kwargs, self._process_group)
        elif func == torch.nn.init.normal_:
            return normal_(types, args, kwargs)
        elif func == torch.nn.init.uniform_:
            return uniform_(types, args, kwargs)
        elif func == torch.nn.init.kaiming_uniform_:
            return kaiming_uniform_(types, args, kwargs)
        raise RuntimeError(
            f"torch function '{func.__name__}', with args: {args} and "
            f"kwargs: {kwargs} not supported for ShardedTensor!")

    def metadata(self) -> ShardedTensorMetadata:
        """
        Returns a :class:`ShardedTensorMetadata` object corresponding to the
        metadata for the entire tensor.
        """
        return self._metadata

    def local_shards(self) -> List[Shard]:
        """
        Returns a list of :class:`Shard' corresponding to the
        local shards for this rank. Returns an empty list if the current rank
        does not host any shards for this Tensor.
        """
        return self._local_shards

    def size(self, dim: int = None) -> Union[torch.Size, int]:
        """
        Returns a :Union:`[torch.Size, int]` which represents the size of the tensor.
            The dimension can be specified.

        Args:
            dim (int, optional): the dimension over which the size represents.
                If specified, it returns the size of the given dimension.
                If not, it returns a subclass of tuple.
                Default: ``None``

        Returns:
            A :Union:`[torch.Size, int]` represents the size of the tensor.
        """
        size = self._metadata.size
        if dim is None:
            return size
        if dim < 0 or dim >= len(size):
            raise ValueError(
                f"Argument ``dim`` must be within the range of tensor dimensions [0, {len(size)})"
            )
        return size[dim]


    def is_pinned(self) -> bool:
        """
        Returns True if the sharded tensor (each local shard) resides in pinned memory.
        """
        return self._metadata.tensor_properties.pin_memory

    def is_contiguous(self) -> bool:
        """
        Returns True if the sharded tensor (each local shard) is contiguous in memory
        in the order specified by memory format.
        """
        return self._metadata.tensor_properties.memory_format == torch.contiguous_format

    @property
    def shape(self):
        return self._metadata.size

    @property
    def requires_grad(self):
        return self._metadata.tensor_properties.requires_grad

    @property
    def dtype(self):
        return self._metadata.tensor_properties.dtype

    @property
    def layout(self):
        return self._metadata.tensor_properties.layout

    def _register_remote_shards(self, remote_shards: List[rpc.RRef[Shard]], rpc_rank: int):
        self._remote_shards[rpc_rank] = remote_shards

    def remote_shards(self) -> Dict[int, List[rpc.RRef[Shard]]]:
        """
        Returns a Dict[int, RRef] with keys being the RPC rank and values
        being RRefs to shards on that rank. Need to initialize the
        RPC framework for this functionality.

        Raises an exception if ShardedTensor was created with ``init_rrefs=False``
        """
        if not self._init_rrefs:
            raise RuntimeError(
                'ShardedTensor created with init_rrefs=False, no RRefs to remote shards available'
            )
        return self._remote_shards

    def __repr__(self):
        return f'ShardedTensor({self._metadata})'

    @dataclass
    class ProcessGroupState:
        """
        State for ser-de of process group
        """
        local_rank: int
        global_rank: int
        local_world_size: int
        global_world_size: int

    def __getstate__(self):
        pg_state = ShardedTensor.ProcessGroupState(
            distributed_c10d.get_rank(self._process_group),
            distributed_c10d.get_rank(),
            distributed_c10d.get_world_size(self._process_group),
            distributed_c10d.get_world_size(),
        )

        return self._local_shards, self._metadata, pg_state, self._sharding_spec, self._init_rrefs

    def __setstate__(self, state):
        self._sharded_tensor_id = None
        if not distributed_c10d.is_initialized():
            raise RuntimeError(
                'Need to initialize default process group using '
                '"init_process_group" before loading ShardedTensor')

        self._local_shards, self._metadata, pg_state, self._sharding_spec, self._init_rrefs = state

        # Setup process group
        self._process_group = get_current_process_group()

        # Validate process group.
        local_rank = distributed_c10d.get_rank(self._process_group)
        if pg_state.local_rank != local_rank:
            raise RuntimeError(
                f'Local rank at save time was {pg_state.local_rank}, but at '
                f'load time was {local_rank}')

        global_rank = distributed_c10d.get_rank()
        if pg_state.global_rank != global_rank:
            raise RuntimeError(
                f'Global rank at save time was {pg_state.global_rank}, but at '
                f'load time was {global_rank}')

        local_world_size = distributed_c10d.get_world_size(self._process_group)
        if pg_state.local_world_size != local_world_size:
            raise RuntimeError(
                f'Local world size at save time was {pg_state.local_world_size}, '
                f'but at load time was {local_world_size}')

        global_world_size = distributed_c10d.get_world_size()
        if pg_state.global_world_size != global_world_size:
            raise RuntimeError(
                f'Global world size at save time was {pg_state.global_world_size}, '
                f'but at load time was {global_world_size}')

        self._post_init()


def _create_tensor_from_params(*size, local_device, tensor_init_params: TensorInitParams):
    """ Helper to construct tensor from size, device and common params. """

    create_op = tensor_init_params.create_op
    dtype = tensor_init_params.tensor_properties.dtype
    layout = tensor_init_params.tensor_properties.layout
    requires_grad = tensor_init_params.tensor_properties.requires_grad
    memory_format = tensor_init_params.tensor_properties.memory_format
    pin_memory = tensor_init_params.tensor_properties.pin_memory

    if create_op == CreateOp.ONES:
        return torch.ones(*size, dtype=dtype, layout=layout,
                          device=local_device, pin_memory=pin_memory,
                          requires_grad=requires_grad,)
    elif create_op == CreateOp.EMPTY:
        return torch.empty(*size, dtype=dtype, layout=layout,
                           device=local_device, requires_grad=requires_grad,
                           # NB: memory_format param is not accepted by torch.ones
                           memory_format=memory_format, pin_memory=pin_memory,)
    elif tensor_init_params.create_op == CreateOp.ZEROS:
        return torch.zeros(*size,
                           dtype=dtype,
                           layout=layout,
                           device=local_device,
                           pin_memory=pin_memory,
                           requires_grad=requires_grad,)
    elif tensor_init_params.create_op == CreateOp.RAND:
        return torch.rand(*size,
                          dtype=dtype,
                          layout=layout,
                          device=local_device,
                          pin_memory=pin_memory,
                          requires_grad=requires_grad,)
    elif tensor_init_params.create_op == CreateOp.FULL:
        return torch.full(size=size,
                          fill_value=tensor_init_params.fill_value,
                          layout=layout,
                          dtype=dtype,
                          requires_grad=requires_grad,
                          device=local_device, )
    else:
        raise ValueError(f'Unsupported create_op: {tensor_init_params.create_op}')
