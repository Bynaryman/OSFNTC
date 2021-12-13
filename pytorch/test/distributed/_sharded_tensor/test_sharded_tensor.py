# Owner(s): ["oncall: distributed"]

import math
import io
import itertools
import pickle
import sys
import torch
import torch.distributed as dist
from torch.distributed import rpc
from torch.distributed import _sharded_tensor
from torch.distributed._sharded_tensor import (
    load_with_process_group,
    pre_load_state_dict_hook,
    shard_parameter,
    state_dict_hook,
)
from torch.distributed._sharding_spec import (
    ChunkShardingSpec,
    EnumerableShardingSpec,
    ShardMetadata
)
from torch.distributed._sharded_tensor.api import (
    CreateOp,
    ShardedTensor,
    TensorInitParams,
    TensorProperties,
    _create_tensor_from_params,
)
from torch.testing._internal.common_distributed import (
    requires_nccl,
    skip_if_lt_x_gpu,
)
from torch.testing._internal.common_utils import (
    TestCase,
    TEST_WITH_DEV_DBG_ASAN,
    run_tests,
    sandcastle_skip_if,
)
from torch.testing._internal.distributed._sharded_tensor import (
    ShardedTensorTestBase,
    with_comms,
)

if TEST_WITH_DEV_DBG_ASAN:
    print("Skip dev-asan as torch + multiprocessing spawn have known issues", file=sys.stderr)
    sys.exit(0)

class MyShardedModel2(torch.nn.Module):
    def __init__(self, spec=None, group=None):
        super(MyShardedModel2, self).__init__()
        if spec is not None:
            self.sharded_tensor2 = _sharded_tensor.empty(spec, 10, 20, process_group=group, init_rrefs=True)
        else:
            self.sharded_tensor2 = None
        self.random_tensor2 = torch.nn.Parameter(torch.rand(2, 2))


class MyShardedModel1(torch.nn.Module):
    def __init__(self, spec=None, group=None):
        super(MyShardedModel1, self).__init__()
        if spec is not None:
            self.sharded_tensor1 = _sharded_tensor.empty(spec, 10, 20, process_group=group, init_rrefs=True)
        else:
            self.sharded_tensor1 = None
        self.random_tensor1 = torch.nn.Parameter(torch.rand(2, 2))
        self.submodule = MyShardedModel2(spec, group)

class TestShardedTensorMetadata(TestCase):
    def test_serialize_and_deserialize(self):
        shard_metadatas = [
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[0, 5],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:2/cuda:2",
            ),
            ShardMetadata(
                shard_offsets=[5, 5],
                shard_sizes=[5, 5],
                placement="rank:3/cuda:3",
            )
        ]

        dtypes = [
            torch.float, torch.double, torch.cfloat, torch.cdouble, torch.half,
            torch.bfloat16, torch.uint8, torch.int8, torch.short, torch.int,
            torch.long, torch.bool]

        layouts = [torch.strided, torch.sparse_coo]
        requires_grads = [True, False]
        memory_formats = [torch.contiguous_format, torch.channels_last, torch.preserve_format]
        pin_memories = [True, False]

        for tensor_properties_input in itertools.product(dtypes, layouts, requires_grads, memory_formats, pin_memories):
            dtype, layout, requires_grad, memory_format, pin_memory = tensor_properties_input

            expected_st_metadata = _sharded_tensor.ShardedTensorMetadata(
                shard_metadatas,
                (10, 10),
                _sharded_tensor.TensorProperties(dtype, layout, requires_grad, memory_format, pin_memory)
            )

            pickled_obj = pickle.dumps(expected_st_metadata)
            st_metadata = pickle.loads(pickled_obj)
            self.assertEqual(expected_st_metadata, st_metadata)

class TestCreateTensorFromParams(TestCase):
    @sandcastle_skip_if(torch.cuda.device_count() < 1, 'CUDA GPU is needed')
    def test_empty(self):
        expected_dtype = torch.double
        tensor_properties = TensorProperties(
            dtype=expected_dtype,
            layout=torch.strided,
            requires_grad=False,
            pin_memory=False,
            memory_format=torch.contiguous_format, )
        tensor_init_params = TensorInitParams(create_op=CreateOp.EMPTY,
                                              tensor_properties=tensor_properties)
        local_device = torch.device('cuda:0')
        local_tensor = _create_tensor_from_params(
            5, 10, local_device=local_device, tensor_init_params=tensor_init_params)
        self.assertEqual(local_device, local_tensor.device)
        self.assertEqual(expected_dtype, local_tensor.dtype)
        self.assertEqual(torch.strided, local_tensor.layout)
        self.assertEqual(False, local_tensor.requires_grad)

    @sandcastle_skip_if(torch.cuda.device_count() < 1, 'CUDA GPU is needed')
    def test_ones(self):
        expected_dtype = torch.double
        tensor_properties = TensorProperties(
            dtype=expected_dtype,
            layout=torch.strided,
            requires_grad=False,
            pin_memory=False,
            memory_format=torch.contiguous_format, )
        tensor_init_params = TensorInitParams(
            create_op=CreateOp.ONES, tensor_properties=tensor_properties)
        local_device = torch.device('cuda:0')
        h, w = 5, 10
        local_tensor = _create_tensor_from_params(
            h, w, local_device=local_device, tensor_init_params=tensor_init_params)
        expected_tensor = torch.ones(h, w, device=local_device, dtype=expected_dtype)
        self.assertEqual(expected_tensor, local_tensor)

    @sandcastle_skip_if(torch.cuda.device_count() < 1, 'CUDA GPU is needed')
    def test_zeros(self):
        expected_dtype = torch.int32
        tensor_properties = TensorProperties(
            dtype=expected_dtype,
            layout=torch.strided,
            requires_grad=False,
            pin_memory=False,
            memory_format=torch.contiguous_format,
        )
        tensor_init_params = TensorInitParams(create_op=CreateOp.ZEROS, tensor_properties=tensor_properties, )
        local_device = torch.device('cuda:0')
        h, w = 5, 10
        local_tensor = _create_tensor_from_params(
            h, w, local_device=local_device, tensor_init_params=tensor_init_params)
        expected_tensor = torch.zeros(h, w, device=local_device, dtype=expected_dtype)
        self.assertEqual(expected_tensor, local_tensor)

    @sandcastle_skip_if(torch.cuda.device_count() < 1, 'CUDA GPU is needed')
    def test_rand(self):
        expected_dtype = torch.double
        tensor_properties = TensorProperties(
            dtype=expected_dtype,
            layout=torch.strided,
            requires_grad=False,
            pin_memory=False,
            memory_format=torch.contiguous_format,
        )
        tensor_init_params = TensorInitParams(create_op=CreateOp.RAND, tensor_properties=tensor_properties, )
        local_device = torch.device('cuda:0')
        h, w = 5, 10
        seed = 13
        torch.cuda.manual_seed(seed)
        local_tensor = _create_tensor_from_params(
            h, w, local_device=local_device, tensor_init_params=tensor_init_params)
        # reset seed to ensure same random numbers are generated
        torch.cuda.manual_seed(seed)
        expected_tensor = torch.rand(h, w, device=local_device, dtype=expected_dtype)
        self.assertEqual(expected_tensor, local_tensor)

    @sandcastle_skip_if(torch.cuda.device_count() < 1, 'CUDA GPU is needed')
    def test_full_with_dtype_inferred(self):
        fill_value = 23.5
        tensor_properties = TensorProperties(
            # tensor's dtype can be inferred from fill_value
            dtype=None,
            layout=torch.strided,
            requires_grad=False,
            pin_memory=False,
            memory_format=torch.contiguous_format,
        )
        tensor_init_params = TensorInitParams(
            create_op=CreateOp.FULL,
            fill_value=fill_value,
            tensor_properties=tensor_properties, )
        local_device = torch.device('cuda:0')
        h, w = 5, 10
        local_tensor = _create_tensor_from_params(
            h, w, local_device=local_device, tensor_init_params=tensor_init_params)
        # local_tensor.dtype is inferred from fill_value (float32).
        self.assertEqual(torch.float32, local_tensor.dtype)
        expected_tensor = torch.full((h, w), fill_value=fill_value, device=local_device)
        self.assertEqual(expected_tensor, local_tensor)

    @sandcastle_skip_if(torch.cuda.device_count() < 1, 'CUDA GPU is needed')
    def test_full_with_dtype_overridden(self):
        fill_value = 23.5
        tensor_properties = TensorProperties(
            # tensor's dtype can be inferred from fill_value
            dtype=torch.double,
            layout=torch.strided,
            requires_grad=False,
            pin_memory=False,
            memory_format=torch.contiguous_format,
        )
        tensor_init_params = TensorInitParams(
            create_op=CreateOp.FULL,
            fill_value=fill_value,
            tensor_properties=tensor_properties, )
        local_device = torch.device('cuda:0')
        h, w = 5, 10
        local_tensor = _create_tensor_from_params(
            h, w, local_device=local_device, tensor_init_params=tensor_init_params)
        # local_tensor.dtype is overridden.
        self.assertEqual(torch.double, local_tensor.dtype)
        expected_tensor = torch.full((h, w), fill_value=fill_value, device=local_device, dtype=torch.double)
        self.assertEqual(expected_tensor, local_tensor)


class TestShardParameter(ShardedTensorTestBase):
    @with_comms(init_rpc=False)
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_shard_parameter(self):
        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )

        fc = torch.nn.Linear(12, 12).cuda(self.rank)
        weight_og = fc.weight.clone()
        shard_parameter(fc, 'weight', spec)

        # Verify.
        self.assertTrue(isinstance(fc.weight, _sharded_tensor.ShardedTensor))
        local_shards = fc.weight.local_shards()
        self.assertEqual(1, len(local_shards))
        self.assertEqual(torch.Size([3, 12]), local_shards[0].tensor.size())
        self.assertEqual(3, local_shards[0].tensor.size(0))
        self.assertEqual(12, local_shards[0].tensor.size(1))
        self.assertEqual(torch.narrow(weight_og, 0, 3 * self.rank, 3), local_shards[0].tensor)

    @with_comms(init_rpc=False)
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_shard_parameter_errors(self):
        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )

        fc = torch.nn.Linear(12, 12).cuda(self.rank)
        with self.assertRaisesRegex(ValueError, 'does not match with src_rank'):
            shard_parameter(fc, 'weight', spec, src_rank=self.rank)

        with self.assertRaisesRegex(ValueError, 'does not have parameter'):
            shard_parameter(fc, 'foo', spec)

        with self.assertRaisesRegex(ValueError, 'Expected Linear.bias to be a Tensor, but found str'):
            del fc.bias
            fc.bias = "foo"
            shard_parameter(fc, 'bias', spec)

        with self.assertRaisesRegex(ValueError, 'not a contiguous Tensor'):
            fc.bias = torch.rand(10, 10).cuda(self.rank).t()
            shard_parameter(fc, 'bias', spec)

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                f"rank:{self.rank}/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )
        with self.assertRaisesRegex(ValueError, 'does not match with sharding_spec'):
            shard_parameter(fc, 'weight', spec)

        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            ),
        ])
        with self.assertRaisesRegex(ValueError, 'Only ChunkShardingspec is supported.'):
            shard_parameter(fc, 'weight', spec)


class TestShardedTensorChunked(ShardedTensorTestBase):

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_sharded_tensor_metadata(self):
        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )

        sharded_tensor = _sharded_tensor.empty(spec, 10, 20, init_rrefs=True)
        sharded_tensor_metadata = sharded_tensor.metadata()
        self.assertEqual(torch.Size([10, 20]), sharded_tensor_metadata.size)
        self.assertEqual(torch.float, sharded_tensor.dtype)
        self.assertEqual(torch.strided, sharded_tensor.layout)
        self.assertEqual(False, sharded_tensor.requires_grad)
        self.assertTrue(sharded_tensor.is_contiguous())
        self.assertFalse(sharded_tensor.is_pinned())

        sharded_tensor = _sharded_tensor.empty(spec, 10, 20, requires_grad=True, init_rrefs=True)
        self.assertEqual(True, sharded_tensor.requires_grad)

        sharded_tensor = _sharded_tensor.empty(spec, 10, 20, dtype=torch.double, init_rrefs=True)
        self.assertEqual(torch.double, sharded_tensor.dtype)

        # Need CPU for pin_memory
        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cpu",
                "rank:1/cpu",
                "rank:2/cpu",
                "rank:3/cpu",
            ],
        )

        sharded_tensor = _sharded_tensor.empty(spec, 10, 20, pin_memory=True, init_rrefs=True)
        self.assertEqual(True, sharded_tensor.is_pinned())

        # test read only properties, they're read only as we can't simply change
        # the global metadata without changing the underlying shard's properties
        with self.assertRaisesRegex(AttributeError, "can't set attribute"):
            sharded_tensor.requires_grad = True

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_complete_world_size(self):

        for dim in [0, -2]:
            spec = ChunkShardingSpec(
                dim=dim,
                placements=[
                    "rank:0/cuda:0",
                    "rank:1/cuda:1",
                    "rank:2/cuda:2",
                    "rank:3/cuda:3",
                ],
            )
            sharded_tensor = _sharded_tensor.empty(spec, 10, 20, init_rrefs=True)

            # Validate local shard.
            local_shards = sharded_tensor.local_shards()
            self.assertEqual(1, len(local_shards))
            local_shard = local_shards[0].tensor
            self.assertEqual(torch.device(f"cuda:{self.rank}"), local_shard.device)
            if self.rank == 3:
                self.assertEqual((1, 20), local_shard.size())
            else:
                self.assertEqual((3, 20), local_shard.size())

            # Validate global metadata.
            sharded_tensor_metadata = sharded_tensor.metadata()
            shards_metadata = sharded_tensor_metadata.shards_metadata
            self.assertEqual(4, len(shards_metadata))

            for rank, shard_metadata in enumerate(shards_metadata):
                self.assertEqual([rank * 3, 0], shard_metadata.shard_offsets)
                if rank == 3:
                    self.assertEqual([1, 20], shard_metadata.shard_sizes)
                else:
                    self.assertEqual([3, 20], shard_metadata.shard_sizes)
                self.assertEqual(f'rank:{rank}/cuda:{rank}', str(shard_metadata.placement))

            # Validate remote shards.
            remote_shards = sharded_tensor.remote_shards()
            self.assertEqual(3, len(remote_shards))

            for rpc_rank, shards in remote_shards.items():
                self.assertEqual(1, len(shards))
                for remote_shard in shards:
                    self.assertEqual(rpc_rank, remote_shard.owner().id)
                    shard = remote_shard.to_here()
                    self.assertEqual(f'rank:{rpc_rank}/cuda:{rpc_rank}', str(shard.metadata.placement))
                    if rpc_rank == 3:
                        self.assertEqual((1, 20), shard.tensor.size())
                    else:
                        self.assertEqual((3, 20), shard.tensor.size())


    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_create_sharded_tensor_with_ones(self):
        """ Test _sharded_tensor.ones(...) """

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )
        h, w = 10, 20
        sharded_tensor = _sharded_tensor.ones(spec, h, w)

        # Validate local shard is initialized with torch.ones
        local_shards = sharded_tensor.local_shards()
        self.assertEqual(1, len(local_shards))
        local_shard = local_shards[0].tensor
        self.assertEqual(torch.device(f"cuda:{self.rank}"), local_shard.device)
        # The split: for rank!=3 ceil(h/4)=3  for rank=3 1
        expected_h = 1 if self.rank == 3 else math.ceil(h / 4)
        self.assertEqual((expected_h, w), local_shard.size())
        self.assertEqual(local_shard, torch.ones(expected_h, w))

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_gather_even(self) -> None:
        """ Test _sharded_tensor.gather(...) with evenly distributed shards"""

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )
        h, w = 10, 20
        sharded_tensor = _sharded_tensor.ones(spec, h, w)

        full_tensor = None
        dst = 1
        if self.rank == dst:
            full_tensor = torch.zeros(
                h,
                w,
                device=torch.device(f"cuda:{dst}"),
            )
        sharded_tensor.gather(dst, full_tensor)

        if self.rank == dst:
            self.assertEqual(full_tensor, torch.ones(h, w))
        else:
            self.assertIsNone(full_tensor)

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_gather_uneven(self) -> None:
        """ Test _sharded_tensor.gather(...) with unevenly distributed shards"""

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
            ],
        )
        h, w = 10, 20
        sharded_tensor = _sharded_tensor.ones(spec, h, w)

        full_tensor = None
        dst = 1
        if self.rank == dst:
            full_tensor = torch.zeros(
                h,
                w,
                device=torch.device(f"cuda:{dst}"),
            )
        sharded_tensor.gather(dst, full_tensor)

        if self.rank == dst:
            self.assertEqual(full_tensor, torch.ones(h, w))
        else:
            self.assertIsNone(full_tensor)

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_create_sharded_tensor_with_zeros(self):
        """ Test _sharded_tensor.zeros(...) """

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )
        h, w = 10, 20
        sharded_tensor = _sharded_tensor.zeros(spec, h, w)

        # Validate local shard is initialized with torch.zeros
        local_shards = sharded_tensor.local_shards()
        self.assertEqual(1, len(local_shards))
        local_shard = local_shards[0].tensor
        self.assertEqual(torch.device(f"cuda:{self.rank}"), local_shard.device)
        # The split: for rank!=3 ceil(h/4)=3  for rank=3 1
        expected_h = 1 if self.rank == 3 else math.ceil(h / 4)
        self.assertEqual((expected_h, w), local_shard.size())
        self.assertEqual(local_shard, torch.zeros(expected_h, w))


    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_create_sharded_tensor_with_rand(self):
        """ Test _sharded_tensor.rand(...) """

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )
        h, w = 8, 2
        seed = 1234

        expected_h = 2
        expected_device = torch.device(f"cuda:{self.rank}")
        dtype = torch.double
        torch.manual_seed(seed)
        expected = torch.rand(expected_h, w, device=expected_device, dtype=dtype)
        # reset seed to ensure the same random numbers are generated
        torch.manual_seed(seed)
        sharded_tensor = _sharded_tensor.rand(spec, h, w, dtype=dtype)

        # Validate local shard is initialized with torch.rand
        local_shards = sharded_tensor.local_shards()
        self.assertEqual(1, len(local_shards))
        local_shard = local_shards[0].tensor
        self.assertEqual(expected_device, local_shard.device)
        self.assertEqual((expected_h, w), local_shard.size())
        self.assertEqual(expected, local_shard)


    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_create_sharded_tensor_with_full(self):
        """ Test _sharded_tensor.full(...) """

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )
        h, w = 10, 20
        fill_value = 1234
        sharded_tensor = _sharded_tensor.full(spec, size=(h, w), fill_value=fill_value, dtype=torch.int32)

        # Validate local shard is initialized with torch.full
        local_shards = sharded_tensor.local_shards()
        self.assertEqual(1, len(local_shards))
        local_shard = local_shards[0].tensor
        self.assertEqual(torch.device(f"cuda:{self.rank}"), local_shard.device)
        # The split: for rank!=3 ceil(h/4)=3  for rank=3 1
        expected_h = 1 if self.rank == 3 else math.ceil(h / 4)
        self.assertEqual((expected_h, w), local_shard.size())
        self.assertEqual(local_shard,
                         torch.full(size=(expected_h, w), fill_value=fill_value, dtype=torch.int32))


    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_partial_world_size(self):

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )
        sharded_tensor = _sharded_tensor.empty(spec, 10, 20, init_rrefs=True)

        # Validate local shard.
        local_shards = sharded_tensor.local_shards()
        if self.rank >= 2:
            self.assertEqual(1, len(local_shards))
            local_shard = local_shards[0].tensor
            self.assertEqual(torch.device(f"cuda:{self.rank}"), local_shard.device)
            self.assertEqual((5, 20), local_shard.size())
        else:
            self.assertEqual(0, len(local_shards))

        # Validate global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(2, len(shards_metadata))

        for shard_rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual([shard_rank * 5, 0], shard_metadata.shard_offsets)
            self.assertEqual([5, 20], shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{shard_rank + 2}/cuda:{shard_rank + 2}', str(shard_metadata.placement))

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards()
        if self.rank >= 2:
            self.assertEqual(1, len(remote_shards))
        else:
            self.assertEqual(2, len(remote_shards))

        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(1, len(shards))
            for remote_shard in shards:
                self.assertEqual(rpc_rank, remote_shard.owner().id)
                shard = remote_shard.to_here()
                self.assertEqual(f'rank:{rpc_rank}/cuda:{rpc_rank}', str(shard.metadata.placement))
                self.assertEqual((5, 20), shard.tensor.size())

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_new_group(self):

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:1/cuda:2",
                "rank:2/cuda:3",
            ],
        )

        pg = dist.new_group(ranks=[1, 2, 3])
        sharded_tensor = _sharded_tensor.empty(spec, 10, 20, process_group=pg, init_rrefs=True)

        # Validate local shard.
        local_shards = sharded_tensor.local_shards()
        if self.rank >= 2:
            self.assertEqual(1, len(local_shards))
            local_shard = local_shards[0].tensor
            self.assertEqual(torch.device(f"cuda:{self.rank}"), local_shard.device)
            self.assertEqual((5, 20), local_shard.size())
        else:
            self.assertEqual(0, len(local_shards))

        # Validate global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(2, len(shards_metadata))

        for shard_rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual([shard_rank * 5, 0], shard_metadata.shard_offsets)
            self.assertEqual([5, 20], shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{shard_rank + 1}/cuda:{shard_rank + 2}', str(shard_metadata.placement))

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards()
        if self.rank >= 2:
            self.assertEqual(1, len(remote_shards))
        else:
            self.assertEqual(2, len(remote_shards))

        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(1, len(shards))
            for remote_shard in shards:
                shard = remote_shard.to_here()
                self.assertEqual(rpc_rank, remote_shard.owner().id)
                self.assertEqual(f'rank:{rpc_rank - 1}/cuda:{rpc_rank}', str(shard.metadata.placement))
                self.assertEqual((5, 20), shard.tensor.size())

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_multiple_local_shards(self):
        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )
        sharded_tensor = _sharded_tensor.empty(spec, 16, 20, init_rrefs=True)

        # Validate local shards.
        local_shards = sharded_tensor.local_shards()
        self.assertEqual(2, len(local_shards))
        for local_shard in local_shards:
            self.assertEqual(torch.device(f"cuda:{self.rank}"), local_shard.tensor.device)
            self.assertEqual((2, 20), local_shard.tensor.size())

        # Validate global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(8, len(shards_metadata))

        for shard_idx, shard_metadata in enumerate(shards_metadata):
            self.assertEqual([shard_idx * 2, 0], shard_metadata.shard_offsets)
            self.assertEqual([2, 20], shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{shard_idx % 4}/cuda:{shard_idx % 4}', str(shard_metadata.placement))

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards()
        self.assertEqual(3, len(remote_shards))
        owners = {}
        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(2, len(shards))
            for remote_shard in shards:
                shard = remote_shard.to_here()
                self.assertEqual((2, 20), shard.tensor.size())
                self.assertEqual(rpc_rank, remote_shard.owner().id)


    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_sharding_columns(self):
        self.init_pg()

        for dim in [1, -1]:
            spec = ChunkShardingSpec(
                dim=dim,
                placements=[
                    "rank:0/cuda:0",
                    "rank:1/cuda:1",
                    "rank:2/cuda:2",
                    "rank:3/cuda:3",
                ],
            )

            sharded_tensor = _sharded_tensor.empty(spec, 10, 32)

            # Validate local shard.
            local_shards = sharded_tensor.local_shards()
            self.assertEqual(1, len(local_shards))
            local_shard = local_shards[0].tensor
            self.assertEqual(torch.device(f"cuda:{self.rank}"), local_shard.device)
            self.assertEqual((10, 8), local_shard.size())

            # Validate global metadata.
            sharded_tensor_metadata = sharded_tensor.metadata()
            shards_metadata = sharded_tensor_metadata.shards_metadata
            self.assertEqual(4, len(shards_metadata))

            for rank, shard_metadata in enumerate(shards_metadata):
                self.assertEqual([0, rank * 8], shard_metadata.shard_offsets)
                self.assertEqual([10, 8], shard_metadata.shard_sizes)
                self.assertEqual(f'rank:{rank}/cuda:{rank}', str(shard_metadata.placement))

    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_invalid_sharding(self):
        self.init_pg()

        spec = ChunkShardingSpec(dim='H', placements=["rank:1/cuda:1"])
        with self.assertRaisesRegex(ValueError, 'needs to be an integer'):
            _sharded_tensor.empty(spec, 10, 20)

        for dim in [2, 3, 4, -3, -4, -5]:
            spec = ChunkShardingSpec(dim=dim, placements=["rank:1/cuda:1"])
            with self.assertRaisesRegex(ValueError, 'Invalid sharding dim'):
                _sharded_tensor.empty(spec, 10, 20)

        spec = ChunkShardingSpec(dim=0, placements=["rank:5/cuda:1"])
        with self.assertRaisesRegex(ValueError, 'Invalid rank'):
            _sharded_tensor.empty(spec, 10, 20)

        spec = ChunkShardingSpec(dim=0, placements=["rank:0/cuda:1"])
        sharded_tensor = _sharded_tensor.empty(spec, 10, 20)
        tensor = torch.empty(10, 20)
        with self.assertRaisesRegex(RuntimeError, "not supported for ShardedTensor!"):
            torch.add(sharded_tensor, tensor)

        spec = ChunkShardingSpec(dim=0, placements=["rank:0/cuda:1"])
        with self.assertRaisesRegex(ValueError, 'Only torch.strided layout is currently supported'):
            _sharded_tensor.empty(spec, 10, 20, layout=torch.sparse)

        spec = ChunkShardingSpec(dim=0, placements=["rank:0/cuda:1"])
        with self.assertRaisesRegex(ValueError, 'Only torch.contiguous_format memory_format is currently supported'):
            _sharded_tensor.empty(spec, 10, 20, memory_format=torch.channels_last)

        spec = ChunkShardingSpec(dim=0, placements=["worker0/cuda:1"])
        with self.assertRaisesRegex(RuntimeError, 'RPC framework needs to be initialized'):
            _sharded_tensor.empty(spec, 10, 20)

        spec = ChunkShardingSpec(dim=0, placements=["rank:0/cuda:1"])
        with self.assertRaisesRegex(RuntimeError, 'RPC Framework needs to be initialized'):
            st = _sharded_tensor.empty(spec, 10, 20, init_rrefs=True)

        with self.assertRaisesRegex(RuntimeError, 'ShardedTensor created with init_rrefs=False'):
            st = _sharded_tensor.empty(spec, 10, 20)
            st.remote_shards()

        self.init_rpc()
        spec = ChunkShardingSpec(dim=0, placements=["workerfoo/cuda:1"])
        with self.assertRaisesRegex(ValueError, 'Invalid worker name'):
            _sharded_tensor.empty(spec, 10, 20, init_rrefs=True)

    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_invalid_pg_rpc_ranks(self):
        self.init_pg()

        # Init RPC with different ranks.
        rpc_backend_options = rpc.TensorPipeRpcBackendOptions()
        rpc_backend_options.init_method = f"file://{self.file_name}"
        rank = (self.rank + 1) % self.world_size
        rpc.init_rpc(
            name=f'worker{rank}',
            rank=rank,
            world_size=self.world_size,
            rpc_backend_options=rpc_backend_options,
        )

        spec = ChunkShardingSpec(dim=0, placements=["rank:1/cuda:1"])
        with self.assertRaisesRegex(ValueError, 'Default ProcessGroup and RPC ranks must be the same'):
            _sharded_tensor.empty(spec, 10, 20, init_rrefs=True)

    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_insufficient_sharding_dims(self):
        self.init_pg()

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )
        sharded_tensor = _sharded_tensor.empty(spec, 2, 20)

        # Validate local shard.
        local_shards = sharded_tensor.local_shards()
        if self.rank <= 1:
            self.assertEqual(1, len(local_shards))
            local_shard = local_shards[0].tensor
            self.assertEqual(torch.device(f"cuda:{self.rank}"), local_shard.device)
            self.assertEqual((1, 20), local_shard.size())
        else:
            self.assertEqual(0, len(local_shards))

        # Validate global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(2, len(shards_metadata))

        for shard_rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual([shard_rank, 0], shard_metadata.shard_offsets)
            self.assertEqual([1, 20], shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{shard_rank}/cuda:{shard_rank}', str(shard_metadata.placement))

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_sharded_tensor_sizes(self):
        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )

        # Test with *args
        sharded_tensor = _sharded_tensor.empty(spec, 10, 20, init_rrefs=True)
        self.assertEqual(torch.Size([10, 20]), sharded_tensor.size())

        # Test with single *args
        sharded_tensor = _sharded_tensor.empty(spec, 10, init_rrefs=True)
        self.assertEqual(torch.Size([10]), sharded_tensor.size())

        # Test with list
        sharded_tensor = _sharded_tensor.empty(spec, [10, 20], init_rrefs=True)
        self.assertEqual(torch.Size([10, 20]), sharded_tensor.size())

        # Test with tuple
        sharded_tensor = _sharded_tensor.empty(spec, (10, 20), init_rrefs=True)
        self.assertEqual(torch.Size([10, 20]), sharded_tensor.size())

        # Test with row size
        sharded_tensor = _sharded_tensor.empty(spec, (10, 20), init_rrefs=True)
        self.assertEqual(sharded_tensor.size(0), 10)

        # Test with col size
        sharded_tensor = _sharded_tensor.empty(spec, (10, 20), init_rrefs=True)
        self.assertEqual(sharded_tensor.size(1), 20)

        # Test with invalid input
        sharded_tensor = _sharded_tensor.empty(spec, (10, 20), init_rrefs=True)
        with self.assertRaisesRegex(ValueError, 'must be within the range of tensor dimensions \\[0, 2\\)'):
            sharded_tensor.size(-1)
        with self.assertRaisesRegex(ValueError, 'must be within the range of tensor dimensions \\[0, 2\\)'):
            sharded_tensor.size(2)

        with self.assertRaises(TypeError):
            sharded_tensor = _sharded_tensor.empty(spec, 'foo')

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_state_dict(self):
        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )

        m = MyShardedModel1(spec)

        # Test save
        m._register_state_dict_hook(state_dict_hook)
        buffer = io.BytesIO()
        torch.save(m.state_dict(), buffer)

        # Test load.
        module_load = MyShardedModel1()
        module_load._register_load_state_dict_pre_hook(pre_load_state_dict_hook, True)

        buffer.seek(0)
        state_dict_deser = torch.load(buffer)
        module_load.load_state_dict(state_dict_deser, strict=False)

        # Verify after load.
        self.assert_sharded_tensor_equal(m.sharded_tensor1, module_load.sharded_tensor1)
        self.assert_sharded_tensor_equal(m.submodule.sharded_tensor2, module_load.submodule.sharded_tensor2)

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_state_dict_new_group(self):
        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:0/cuda:2",
                "rank:1/cuda:3",
            ],
        )

        pg = dist.new_group([2, 3])

        m = MyShardedModel1(spec, pg)

        # Test save
        m._register_state_dict_hook(state_dict_hook)
        buffer = io.BytesIO()
        torch.save(m.state_dict(), buffer)

        # Test load.
        module_load = MyShardedModel1(spec=None, group=pg)
        module_load._register_load_state_dict_pre_hook(pre_load_state_dict_hook, True)

        buffer.seek(0)
        with load_with_process_group(pg):
            state_dict_deser = torch.load(buffer)
            module_load.load_state_dict(state_dict_deser, strict=False)

        # Verify after load.
        self.assert_sharded_tensor_equal(m.sharded_tensor1, module_load.sharded_tensor1)
        self.assert_sharded_tensor_equal(m.submodule.sharded_tensor2, module_load.submodule.sharded_tensor2)

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_state_dict_no_sharded_tensors(self):
        # Verify hooks don't affect modules with no ShardedTensors.
        m = torch.nn.Linear(10, 10)

        # Test save
        state_dict_before = m.state_dict()
        m._register_state_dict_hook(state_dict_hook)
        buffer = io.BytesIO()
        torch.save(m.state_dict(), buffer)
        self.assertEqual(state_dict_before, m.state_dict())

        # Test load.
        module_load = torch.nn.Linear(10, 10)
        module_load._register_load_state_dict_pre_hook(pre_load_state_dict_hook, True)

        buffer.seek(0)
        state_dict_deser = torch.load(buffer)
        module_load.load_state_dict(state_dict_deser, strict=False)

        # Verify after load.
        self.assertEqual(m.weight, module_load.weight)
        self.assertEqual(m.bias, module_load.bias)

    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_load_state_dict_errors(self):
        self.init_rpc()

        dist.init_process_group(
            backend="nccl",
            world_size=self.world_size,
            rank=self.rank,
            init_method=f"file://{self.file_name}",
        )

        spec = ChunkShardingSpec(
            dim=0,
            placements=[
                "rank:0/cuda:0",
                "rank:1/cuda:1",
                "rank:2/cuda:2",
                "rank:3/cuda:3",
            ],
        )

        m = MyShardedModel1(spec)

        # Test save
        m._register_state_dict_hook(state_dict_hook)
        buffer = io.BytesIO()
        torch.save(m.state_dict(), buffer)

        pg = dist.new_group(ranks=[0, 2, 3])

        buffer.seek(0)
        if self.rank != 0:
            with self.assertRaisesRegex(RuntimeError, 'Local rank at save time was'):
                with load_with_process_group(pg):
                    state_dict_deser = torch.load(buffer)
        else:
            with self.assertRaisesRegex(RuntimeError, 'Local world size at save time was'):
                with load_with_process_group(pg):
                    state_dict_deser = torch.load(buffer)

        dist.destroy_process_group()
        buffer.seek(0)
        with self.assertRaisesRegex(RuntimeError, 'Need to initialize default process group'):
            state_dict_deser = torch.load(buffer)


class TestShardedTensorEnumerable(ShardedTensorTestBase):

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_sharded_tensor_metadata(self):
        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[0, 5],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:2/cuda:2",
            ),
            ShardMetadata(
                shard_offsets=[5, 5],
                shard_sizes=[5, 5],
                placement="rank:3/cuda:3",
            )
        ])

        sharded_tensor = _sharded_tensor.empty(spec, 10, 10, init_rrefs=True)
        sharded_tensor_metadata = sharded_tensor.metadata()
        self.assertEqual(torch.Size([10, 10]), sharded_tensor_metadata.size)
        self.assertEqual(torch.float, sharded_tensor.dtype)
        self.assertEqual(torch.strided, sharded_tensor.layout)
        self.assertEqual(False, sharded_tensor.requires_grad)
        self.assertTrue(sharded_tensor.is_contiguous())
        self.assertFalse(sharded_tensor.is_pinned())

        sharded_tensor = _sharded_tensor.empty(spec, 10, 10, requires_grad=True, init_rrefs=True)
        self.assertEqual(True, sharded_tensor.requires_grad)

        sharded_tensor = _sharded_tensor.empty(spec, 10, 10, dtype=torch.double, init_rrefs=True)
        self.assertEqual(torch.double, sharded_tensor.dtype)

        # Need CPU for pin_memory
        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cpu",
            ),
            ShardMetadata(
                shard_offsets=[0, 5],
                shard_sizes=[5, 5],
                placement="rank:1/cpu",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:2/cpu",
            ),
            ShardMetadata(
                shard_offsets=[5, 5],
                shard_sizes=[5, 5],
                placement="rank:3/cpu",
            )
        ])

        sharded_tensor = _sharded_tensor.empty(spec, 10, 10, pin_memory=True, init_rrefs=True)
        self.assertTrue(sharded_tensor.is_pinned())

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_grid_sharding(self):

        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[0, 5],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:2/cuda:2",
            ),
            ShardMetadata(
                shard_offsets=[5, 5],
                shard_sizes=[5, 5],
                placement="rank:3/cuda:3",
            )
        ])

        sharded_tensor = _sharded_tensor.empty(spec, 10, 10, init_rrefs=True)
        self.assertEqual((10, 10), sharded_tensor.size())
        self.assertEqual(1, len(sharded_tensor.local_shards()))

        # Verify local shard.
        local_shard = sharded_tensor.local_shards()[0]
        self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
        self.assertEqual((5, 5), local_shard.tensor.size())

        # Verify local shard metadata.
        self.assertEqual((self.rank // 2 * 5, (self.rank % 2) * 5), local_shard.metadata.shard_offsets)
        self.assertEqual((5, 5), local_shard.metadata.shard_sizes)
        self.assertEqual(f'rank:{self.rank}/cuda:{self.rank}', str(local_shard.metadata.placement))

        # Verify global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(4, len(shards_metadata))
        for rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual((rank // 2 * 5, (rank % 2) * 5), shard_metadata.shard_offsets)
            self.assertEqual((5, 5), shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{rank}/cuda:{rank}', str(shard_metadata.placement))

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards()
        self.assertEqual(3, len(remote_shards))

        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(1, len(shards))
            for remote_shard in shards:
                self.assertEqual(rpc_rank, remote_shard.owner().id)
                shard = remote_shard.to_here()
                self.assertEqual((5, 5), shard.tensor.size())

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_create_sharded_tensor_with_ones(self):
        """ Test _sharded_tensor.ones(...) """

        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[0, 5],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:2/cuda:2",
            ),
            ShardMetadata(
                shard_offsets=[5, 5],
                shard_sizes=[5, 5],
                placement="rank:3/cuda:3",
            )
        ])

        sharded_tensor = _sharded_tensor.ones(spec, 10, 10, init_rrefs=True)
        self.assertEqual((10, 10), sharded_tensor.size())
        self.assertEqual(1, len(sharded_tensor.local_shards()))

        # Verify local shard is initialized with torch.ones
        local_shard = sharded_tensor.local_shards()[0]
        self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
        self.assertEqual((5, 5), local_shard.tensor.size())
        self.assertEqual(local_shard.tensor, torch.ones(5, 5))

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_gather_even(self) -> None:
        """ Test _sharded_tensor.gather(...) with evenly distributed shards"""

        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[0, 5],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:2/cuda:2",
            ),
            ShardMetadata(
                shard_offsets=[5, 5],
                shard_sizes=[5, 5],
                placement="rank:3/cuda:3",
            )
        ])

        h, w = 10, 10
        sharded_tensor = _sharded_tensor.ones(spec, h, w, init_rrefs=True)

        full_tensor = None
        dst = 0
        if self.rank == dst:
            full_tensor = torch.zeros(
                h,
                w,
                device=torch.device(f"cuda:{dst}")
            )
        sharded_tensor.gather(dst, full_tensor)

        if self.rank == dst:
            self.assertEqual(full_tensor, torch.ones(h, w))
        else:
            self.assertIsNone(full_tensor)

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_gather_uneven(self) -> None:
        """ Test _sharded_tensor.gather(...) with unevenly distributed shards"""

        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[0, 5],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[5, 5],
                shard_sizes=[5, 5],
                placement="rank:3/cuda:3",
            )
        ])

        h, w = 10, 10
        sharded_tensor = _sharded_tensor.ones(spec, h, w, init_rrefs=True)

        full_tensor = None
        dst = 0
        if self.rank == dst:
            full_tensor = torch.zeros(
                h,
                w,
                device=torch.device(f"cuda:{dst}")
            )
        sharded_tensor.gather(dst, full_tensor)

        if self.rank == dst:
            self.assertEqual(full_tensor, torch.ones(h, w))
        else:
            self.assertIsNone(full_tensor)

    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_uneven_shards(self):
        self.init_pg()

        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[2, 4],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[0, 4],
                shard_sizes=[4, 2],
                placement="rank:1/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[2, 0],
                shard_sizes=[4, 4],
                placement="rank:2/cuda:2",
            ),
            ShardMetadata(
                shard_offsets=[4, 4],
                shard_sizes=[2, 2],
                placement="rank:3/cuda:3",
            ),
        ])

        sharded_tensor = _sharded_tensor.empty(spec, 6, 6)
        self.assertEqual((6, 6), sharded_tensor.size())
        self.assertEqual(1, len(sharded_tensor.local_shards()))

        def verify_size(rank, tensor_dims):
            if rank == 0:
                self.assertEqual((2, 4), tensor_dims)
            elif rank == 1:
                self.assertEqual((4, 2), tensor_dims)
            elif rank == 2:
                self.assertEqual((4, 4), tensor_dims)
            elif rank == 3:
                self.assertEqual((2, 2), tensor_dims)

        def verify_offsets(rank, offsets):
            if rank == 0:
                self.assertEqual((0, 0), offsets)
            elif rank == 1:
                self.assertEqual((0, 4), offsets)
            elif rank == 2:
                self.assertEqual((2, 0), offsets)
            elif rank == 3:
                self.assertEqual((4, 4), offsets)

        # Verify local shard.
        local_shard = sharded_tensor.local_shards()[0]
        self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
        verify_size(self.rank, local_shard.tensor.size())

        # Verify local shard metadata.
        verify_offsets(self.rank, local_shard.metadata.shard_offsets)
        verify_size(self.rank, local_shard.metadata.shard_sizes)
        self.assertEqual(f'rank:{self.rank}/cuda:{self.rank}', str(local_shard.metadata.placement))

        # Verify global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(4, len(shards_metadata))
        for rank, shard_metadata in enumerate(shards_metadata):
            verify_offsets(rank, shard_metadata.shard_offsets)
            verify_size(rank, shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{rank}/cuda:{rank}', str(shard_metadata.placement))

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_partial_world_size(self):
        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            ),
        ])

        sharded_tensor = _sharded_tensor.empty(spec, 10, 5, init_rrefs=True)
        self.assertEqual((10, 5), sharded_tensor.size())
        if self.rank <= 1:
            self.assertEqual(1, len(sharded_tensor.local_shards()))
        else:
            self.assertEqual(0, len(sharded_tensor.local_shards()))

        if self.rank <= 1:
            # Verify local shard.
            local_shard = sharded_tensor.local_shards()[0]
            self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
            self.assertEqual((5, 5), local_shard.tensor.size())

            # Verify local shard metadata.
            self.assertEqual((self.rank * 5, 0), local_shard.metadata.shard_offsets)
            self.assertEqual((5, 5), local_shard.metadata.shard_sizes)
            self.assertEqual(f'rank:{self.rank}/cuda:{self.rank}', str(local_shard.metadata.placement))

        # Verify global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(2, len(shards_metadata))
        for rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual((rank * 5, 0), shard_metadata.shard_offsets)
            self.assertEqual((5, 5), shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{rank}/cuda:{rank}', str(shard_metadata.placement))

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards()
        if self.rank <= 1:
            self.assertEqual(1, len(remote_shards))
        else:
            self.assertEqual(2, len(remote_shards))

        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(1, len(shards))

            for remote_shard in shards:
                self.assertEqual(rpc_rank, remote_shard.owner().id)
                shard = remote_shard.to_here()
                self.assertEqual((5, 5), shard.tensor.size())

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_new_group(self):
        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:2/cuda:3",
            ),
        ])

        pg = dist.new_group(ranks=[1, 2, 3])

        sharded_tensor = _sharded_tensor.empty(spec, 10, 5, process_group=pg, init_rrefs=True)
        self.assertEqual((10, 5), sharded_tensor.size())
        if self.rank == 1 or self.rank == 3:
            # Verify local shard.
            local_shard = sharded_tensor.local_shards()[0]
            self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
            self.assertEqual((5, 5), local_shard.tensor.size())

            # Verify local shard metadata.
            self.assertEqual((self.rank // 2 * 5, 0), local_shard.metadata.shard_offsets)
            self.assertEqual((5, 5), local_shard.metadata.shard_sizes)
            self.assertEqual(f'rank:{self.rank - 1}/cuda:{self.rank}', str(local_shard.metadata.placement))

        # Verify global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(2, len(shards_metadata))
        for rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual((rank * 5, 0), shard_metadata.shard_offsets)
            self.assertEqual((5, 5), shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{rank * 2}/cuda:{rank * 2 + 1}', str(shard_metadata.placement))

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards()
        if self.rank == 1 or self.rank == 3:
            self.assertEqual(1, len(remote_shards))
        else:
            self.assertEqual(2, len(remote_shards))

        owners = {}
        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(1, len(shards))

            for remote_shard in shards:
                self.assertEqual(rpc_rank, remote_shard.owner().id)
                shard = remote_shard.to_here()
                self.assertEqual((5, 5), shard.tensor.size())

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_multiple_local_shards(self):
        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[0, 5],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[5, 5],
                shard_sizes=[5, 5],
                placement="rank:1/cuda:1",
            )
        ])

        sharded_tensor = _sharded_tensor.empty(spec, 10, 10, init_rrefs=True)
        self.assertEqual((10, 10), sharded_tensor.size())

        if self.rank <= 1:
            self.assertEqual(2, len(sharded_tensor.local_shards()))

            # Verify local shards.
            for idx, local_shard in enumerate(sharded_tensor.local_shards()):
                self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
                self.assertEqual((5, 5), local_shard.tensor.size())

                # Verify local shard metadata.
                self.assertEqual((idx * 5, self.rank * 5), local_shard.metadata.shard_offsets)
                self.assertEqual((5, 5), local_shard.metadata.shard_sizes)
                self.assertEqual(f'rank:{self.rank}/cuda:{self.rank}', str(local_shard.metadata.placement))
        else:
            self.assertEqual(0, len(sharded_tensor.local_shards()))

        # Verify global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(4, len(shards_metadata))
        for shard_rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual((shard_rank // 2 * 5, (shard_rank % 2) * 5), shard_metadata.shard_offsets)
            self.assertEqual((5, 5), shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{shard_rank % 2}/cuda:{shard_rank % 2}', str(shard_metadata.placement))

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards()
        if self.rank <= 1:
            self.assertEqual(1, len(remote_shards))
        else:
            self.assertEqual(2, len(remote_shards))

        owners = {}
        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(2, len(shards))
            for remote_shard in shards:
                self.assertEqual(rpc_rank, remote_shard.owner().id)
                shard = remote_shard.to_here()
                self.assertEqual((5, 5), shard.tensor.size())

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_with_rpc_names(self):
        spec = EnumerableShardingSpec([
            ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="worker0/cuda:0",
            ),
            ShardMetadata(
                shard_offsets=[0, 5],
                shard_sizes=[5, 5],
                placement="worker1/cuda:1",
            ),
            ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="worker2/cuda:2",
            ),
            ShardMetadata(
                shard_offsets=[5, 5],
                shard_sizes=[5, 5],
                placement="worker3/cuda:3",
            )
        ])

        sharded_tensor = _sharded_tensor.empty(spec, 10, 10, init_rrefs=True)
        self.assertEqual((10, 10), sharded_tensor.size())
        self.assertEqual(1, len(sharded_tensor.local_shards()))

        # Verify local shard.
        local_shard = sharded_tensor.local_shards()[0]
        self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
        self.assertEqual((5, 5), local_shard.tensor.size())

        # Verify local shard metadata.
        self.assertEqual((self.rank // 2 * 5, (self.rank % 2) * 5), local_shard.metadata.shard_offsets)
        self.assertEqual((5, 5), local_shard.metadata.shard_sizes)
        self.assertEqual(f'worker{self.rank}/cuda:{self.rank}', str(local_shard.metadata.placement))

        # Verify global metadata.
        sharded_tensor_metadata = sharded_tensor.metadata()
        shards_metadata = sharded_tensor_metadata.shards_metadata
        self.assertEqual(4, len(shards_metadata))
        for rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual((rank // 2 * 5, (rank % 2) * 5), shard_metadata.shard_offsets)
            self.assertEqual((5, 5), shard_metadata.shard_sizes)
            self.assertEqual(f'worker{rank}/cuda:{rank}', str(shard_metadata.placement))

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards()
        self.assertEqual(3, len(remote_shards))

        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(1, len(shards))
            for remote_shard in shards:
                self.assertEqual(rpc_rank, remote_shard.owner().id)
                shard = remote_shard.to_here()
                self.assertEqual((5, 5), shard.tensor.size())


class TestShardedTensorFromLocalShards(ShardedTensorTestBase):

    @with_comms(init_rpc=False)
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_local_shards(self):
        shard_offsets = [(self.rank // 2) * 5, (self.rank % 2) * 5]
        local_shard_metadata = ShardMetadata(
            shard_offsets=shard_offsets,
            shard_sizes=[5, 5],
            placement=f"rank:{self.rank}/cuda:{self.rank}"
        )

        local_tensor = torch.randn(5, 5, device=f"cuda:{self.rank}")
        local_shard = _sharded_tensor.Shard(local_tensor, local_shard_metadata)
        local_shard_from_offsets = _sharded_tensor.Shard.from_tensor_and_offsets(
            local_tensor,
            shard_offsets=shard_offsets,
            rank=self.rank
        )
        self.assertEqual(local_shard.metadata, local_shard_from_offsets.metadata)

        wrong_local_shard_metadata = ShardMetadata(
            shard_offsets=shard_offsets,
            shard_sizes=[6, 5],
            placement=f"rank:{self.rank}/cuda:{self.rank}"
        )
        with self.assertRaisesRegex(ValueError, 'Shard tensor size does not match'):
            local_shard_from_wrong_meta = _sharded_tensor.Shard(
                local_tensor,
                metadata=wrong_local_shard_metadata,
            )

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_init_from_local_shards(self):
        local_shard_metadata = ShardMetadata(
            shard_offsets=[(self.rank // 2) * 5, (self.rank % 2) * 5],
            shard_sizes=[5, 5],
            placement=f"rank:{self.rank}/cuda:{self.rank}"
        )

        local_shards = [_sharded_tensor.Shard(torch.randn(5, 5, device=f"cuda:{self.rank}"), local_shard_metadata)]

        sharded_tensor = _sharded_tensor.init_from_local_shards(local_shards, [10, 10], init_rrefs=True)
        self.assertEqual((10, 10), sharded_tensor.size())
        self.assertEqual(1, len(sharded_tensor.local_shards()))

        # Verify local shard.
        local_shard = sharded_tensor.local_shards()[0]
        self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
        self.assertEqual((5, 5), local_shard.tensor.size())

        # Verify local shard metadata.
        self.assertEqual((self.rank // 2 * 5, (self.rank % 2) * 5), local_shard.metadata.shard_offsets)
        self.assertEqual((5, 5), local_shard.metadata.shard_sizes)
        self.assertEqual(f'rank:{self.rank}/cuda:{self.rank}', str(local_shard.metadata.placement))

        # Verify global metadata.
        shards_metadata = sharded_tensor.metadata().shards_metadata
        self.assertEqual(4, len(shards_metadata))
        for rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual((rank // 2 * 5, (rank % 2) * 5), shard_metadata.shard_offsets)
            self.assertEqual((5, 5), shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{rank}/cuda:{rank}', str(shard_metadata.placement))

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards()
        self.assertEqual(3, len(remote_shards))

        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(1, len(shards))
            for remote_shard in shards:
                self.assertEqual(rpc_rank, remote_shard.owner().id)
                shard = remote_shard.to_here()
                self.assertEqual((5, 5), shard.tensor.size())


    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_init_from_local_shards_and_global_metadata(self):
        local_shard_metadata = ShardMetadata(
            shard_offsets=[(self.rank // 2) * 5, (self.rank % 2) * 5],
            shard_sizes=[5, 5],
            placement=f"rank:{self.rank}/cuda:{self.rank}"
        )

        shards_metadata = []
        for r in range(self.world_size):
            if r == self.rank:
                shards_metadata.append(local_shard_metadata)
            else:
                shards_metadata.append(ShardMetadata(
                    shard_offsets=[(r // 2) * 5, (r % 2) * 5],
                    shard_sizes=[5, 5],
                    placement=f"rank:{r}/cuda:{r}"
                ))

        local_shards = [_sharded_tensor.Shard(torch.randn(5, 5, device=f"cuda:{self.rank}"), local_shard_metadata)]

        sharded_tensor_metadata = _sharded_tensor.ShardedTensorMetadata(
            shards_metadata=shards_metadata,
            size=torch.Size([10, 10]),
            dtype=torch.get_default_dtype(),
            layout=torch.strided,
            requires_grad=False,
            memory_format=torch.contiguous_format,
            pin_memory=False,
        )

        sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
            local_shards,
            sharded_tensor_metadata
        )
        self.assertEqual((10, 10), sharded_tensor.size())
        self.assertEqual(1, len(sharded_tensor.local_shards()))

        # Verify local shard.
        local_shard = sharded_tensor.local_shards()[0]
        self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
        self.assertEqual((5, 5), local_shard.tensor.size())

        # Verify local shard metadata.
        self.assertEqual((self.rank // 2 * 5, (self.rank % 2) * 5), local_shard.metadata.shard_offsets)
        self.assertEqual((5, 5), local_shard.metadata.shard_sizes)
        self.assertEqual(f'rank:{self.rank}/cuda:{self.rank}', local_shard.metadata.placement)

        # Verify global metadata.
        shards_metadata = sharded_tensor.metadata().shards_metadata
        self.assertEqual(4, len(shards_metadata))
        for rank, shard_metadata in enumerate(shards_metadata):
            self.assertEqual((rank // 2 * 5, (rank % 2) * 5), shard_metadata.shard_offsets)
            self.assertEqual((5, 5), shard_metadata.shard_sizes)
            self.assertEqual(f'rank:{rank}/cuda:{rank}', shard_metadata.placement)

        # Validate remote shards.
        remote_shards = sharded_tensor.remote_shards
        self.assertEqual(3, len(remote_shards))

        for rpc_rank, shards in remote_shards.items():
            self.assertEqual(1, len(shards))
            for remote_shard in shards:
                self.assertEqual(rpc_rank, remote_shard.owner().id)
                shard = remote_shard.to_here()
                self.assertEqual((5, 5), shard.tensor.size())

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_init_from_local_shards_new_group(self):
        new_pg = dist.new_group(ranks=[1, 2, 3])

        if self.rank == 1 or self.rank == 3:
            rank1_shard_metadata = ShardMetadata(
                shard_offsets=[0, 0],
                shard_sizes=[5, 5],
                placement="rank:0/cuda:1"
            )
            rank3_shard_metadata = ShardMetadata(
                shard_offsets=[5, 0],
                shard_sizes=[5, 5],
                placement="rank:2/cuda:3"
            )


            local_shard_metadata = rank1_shard_metadata if self.rank == 1 else rank3_shard_metadata
            local_shards = [_sharded_tensor.Shard(torch.randn(5, 5, device=f"cuda:{self.rank}"), local_shard_metadata)]

            sharded_tensor = _sharded_tensor.init_from_local_shards(local_shards, [10, 5], new_pg, init_rrefs=True)

            # Verify local shard.
            local_shard = sharded_tensor.local_shards()[0]
            self.assertEqual(torch.device(f'cuda:{self.rank}'), local_shard.tensor.device)
            self.assertEqual((5, 5), local_shard.tensor.size())

            # Verify local shard metadata.
            self.assertEqual((self.rank // 2 * 5, 0), local_shard.metadata.shard_offsets)
            self.assertEqual((5, 5), local_shard.metadata.shard_sizes)
            self.assertEqual(f'rank:{self.rank - 1}/cuda:{self.rank}', str(local_shard.metadata.placement))

            # Verify global metadata.
            sharded_tensor_metadata = sharded_tensor.metadata()
            shards_metadata = sharded_tensor_metadata.shards_metadata
            self.assertEqual(2, len(shards_metadata))
            for rank, shard_metadata in enumerate(shards_metadata):
                self.assertEqual((rank * 5, 0), shard_metadata.shard_offsets)
                self.assertEqual((5, 5), shard_metadata.shard_sizes)
                self.assertEqual(f'rank:{rank * 2}/cuda:{rank * 2 + 1}', str(shard_metadata.placement))

            # Validate remote shards.
            remote_shards = sharded_tensor.remote_shards()
            if self.rank == 1 or self.rank == 3:
                self.assertEqual(1, len(remote_shards))
            else:
                self.assertEqual(2, len(remote_shards))

            owners = {}
            for rpc_rank, shards in remote_shards.items():
                self.assertEqual(1, len(shards))

                for remote_shard in shards:
                    self.assertEqual(rpc_rank, remote_shard.owner().id)
                    shard = remote_shard.to_here()
                    self.assertEqual((5, 5), shard.tensor.size())


    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_init_from_local_shards_invalid_local_shards(self):
        local_shard_metadata = ShardMetadata(
            shard_offsets=[(self.rank // 2) * 5, (self.rank % 2) * 5],
            shard_sizes=[5, 5],
            placement=f"rank:{self.rank}/cuda:{self.rank}"
        )

        empty_local_shards = []
        with self.assertRaisesRegex(ValueError, 'have no local shards on all ranks'):
            sharded_tensor = _sharded_tensor.init_from_local_shards(empty_local_shards, [10, 10], init_rrefs=True)

        indices = [[0, 1, 1], [2, 0, 2]]
        values = [3.2, 4.5, 5.8]
        sparse_tensor = torch.sparse_coo_tensor(indices, values, (5, 5), device=f"cuda:{self.rank}")

        wrong_layout_shards = [
            _sharded_tensor.Shard(sparse_tensor, local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, 'Only torch.strided layout is currently supported'):
            sharded_tensor = _sharded_tensor.init_from_local_shards(
                wrong_layout_shards, [10, 10], init_rrefs=True)

        wrong_memory_format_shards = [
            _sharded_tensor.Shard(torch.randn(5, 5, device=f"cuda:{self.rank}").t(), local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, 'Only torch.contiguous_format memory_format is currently supported'):
            sharded_tensor = _sharded_tensor.init_from_local_shards(
                wrong_memory_format_shards, [10, 10], init_rrefs=True)

        wrong_size_shards = [_sharded_tensor.Shard(torch.randn(2, 3, device=f"cuda:{self.rank}"), local_shard_metadata)]
        with self.assertRaisesRegex(ValueError, 'Local shard tensor size does not match'):
            sharded_tensor = _sharded_tensor.init_from_local_shards(wrong_size_shards, [10, 10], init_rrefs=True)

        wrong_device_shards = [_sharded_tensor.Shard(torch.randn(5, 5), local_shard_metadata)]
        with self.assertRaisesRegex(ValueError, "Local shard tensor device does not match with local Shard's placement"):
            sharded_tensor = _sharded_tensor.init_from_local_shards(wrong_device_shards, [10, 10], init_rrefs=True)

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_init_from_local_shards_invalid_property_cross_ranks(self):
        local_shard_metadata = ShardMetadata(
            shard_offsets=[(self.rank // 2) * 5, (self.rank % 2) * 5],
            shard_sizes=[5, 5],
            placement=f"rank:{self.rank}/cuda:{self.rank}"
        )
        tensor_overall_size = [10, 10] if self.rank == 0 else [10, 5]
        wrong_dtype_shards = [
            _sharded_tensor.Shard(torch.ones(5, 5, device=f"cuda:{self.rank}"), local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, "ShardedTensor overall_size does not match from different ranks!"):
            sharded_tensor = _sharded_tensor.init_from_local_shards(wrong_dtype_shards, tensor_overall_size, init_rrefs=True)

        tensor_dtype = torch.int if self.rank == 0 else torch.float32
        wrong_dtype_shards = [
            _sharded_tensor.Shard(torch.ones(5, 5, device=f"cuda:{self.rank}", dtype=tensor_dtype), local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, "ShardedTensor dtype property does not match from different ranks!"):
            sharded_tensor = _sharded_tensor.init_from_local_shards(wrong_dtype_shards, [10, 10], init_rrefs=True)

        tensor_requires_grad = True if self.rank == 0 else False
        wrong_requires_grad_shards = [
            _sharded_tensor.Shard(
                torch.randn(5, 5, device=f"cuda:{self.rank}", requires_grad=tensor_requires_grad),
                local_shard_metadata
            )
        ]
        with self.assertRaisesRegex(ValueError, 'ShardedTensor requires_grad property does not match from different ranks!'):
            sharded_tensor = _sharded_tensor.init_from_local_shards(
                wrong_requires_grad_shards, [10, 10], init_rrefs=True)

        local_shard_metadata = ShardMetadata(
            shard_offsets=[(self.rank // 2) * 5, (self.rank % 2) * 5],
            shard_sizes=[5, 5],
            placement=f"rank:{self.rank}/cpu"
        )

    @with_comms(init_rpc=False, backend="gloo")
    @skip_if_lt_x_gpu(4)
    def test_init_from_local_shards_invalid_pin_memory(self):
        # pin memory can only be on dense cpu
        local_shard_metadata = ShardMetadata(
            shard_offsets=[(self.rank // 2) * 5, (self.rank % 2) * 5],
            shard_sizes=[5, 5],
            placement=f"rank:{self.rank}/cpu"
        )
        wrong_pin_memory_local_shards = [
            _sharded_tensor.Shard(torch.randn(5, 5, pin_memory=True), local_shard_metadata),
            _sharded_tensor.Shard(torch.randn(5, 5, pin_memory=False), local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, "Local shards' tensor pin_memory property need to be the same"):
            sharded_tensor = _sharded_tensor.init_from_local_shards(
                wrong_pin_memory_local_shards, [10, 10], init_rrefs=True)

        tensor_pin_memory = True if self.rank == 0 else False
        wrong_pin_memory_shards_cross_ranks = [
            _sharded_tensor.Shard(torch.randn(5, 5, pin_memory=tensor_pin_memory), local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, 'ShardedTensor pin_memory property does not match from different ranks!'):
            sharded_tensor = _sharded_tensor.init_from_local_shards(
                wrong_pin_memory_shards_cross_ranks, [10, 10], init_rrefs=True)


    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_init_from_local_shards_invalid_shards_overlap(self):
        local_shard_size = [5, 5] if self.rank != 0 else [6, 6]
        local_shard_metadata = ShardMetadata(
            shard_offsets=[(self.rank // 2) * 5, (self.rank % 2) * 5],
            shard_sizes=local_shard_size,
            placement=f"rank:{self.rank}/cuda:{self.rank}"
        )

        local_shards = [_sharded_tensor.Shard(torch.randn(local_shard_size, device=f"cuda:{self.rank}"), local_shard_metadata)]

        with self.assertRaisesRegex(ValueError, "overlap"):
            sharded_tensor = _sharded_tensor.init_from_local_shards(local_shards, [10, 10], init_rrefs=True)


    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_init_from_local_shards_invalid_shards_gaps(self):
        local_shard_size = [5, 5] if self.rank != 0 else [4, 4]
        local_shard_metadata = ShardMetadata(
            shard_offsets=[(self.rank // 2) * 5, (self.rank % 2) * 5],
            shard_sizes=local_shard_size,
            placement=f"rank:{self.rank}/cuda:{self.rank}"
        )

        local_shards = [_sharded_tensor.Shard(torch.randn(local_shard_size, device=f"cuda:{self.rank}"), local_shard_metadata)]

        with self.assertRaisesRegex(ValueError, "does not match tensor volume"):
            sharded_tensor = _sharded_tensor.init_from_local_shards(local_shards, [10, 10], init_rrefs=True)

    @with_comms
    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_init_from_local_shards_and_global_metadata_invalid_shards(self):
        local_shard_metadata = ShardMetadata(
            shard_offsets=[(self.rank // 2) * 5, (self.rank % 2) * 5],
            shard_sizes=[5, 5],
            placement=f"rank:{self.rank}/cuda:{self.rank}"
        )

        shards_metadata = []
        for r in range(self.world_size):
            if r == self.rank:
                shards_metadata.append(local_shard_metadata)
            else:
                shards_metadata.append(ShardMetadata(
                    shard_offsets=[(r // 2) * 5, (r % 2) * 5],
                    shard_sizes=[5, 5],
                    placement=f"rank:{r}/cuda:{r}"
                ))

        sharded_tensor_metadata = _sharded_tensor.ShardedTensorMetadata(
            shards_metadata=shards_metadata,
            size=torch.Size([10, 10]),
            dtype=torch.get_default_dtype(),
            layout=torch.strided,
            requires_grad=False,
            memory_format=torch.contiguous_format,
            pin_memory=False,
        )

        empty_local_shards = []
        with self.assertRaisesRegex(RuntimeError, 'does not match number of local shards metadata'):
            sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
                empty_local_shards,
                sharded_tensor_metadata
            )

        wrong_num_shards = [
            _sharded_tensor.Shard(torch.randn(5, 5, device=f"cuda:{self.rank}"), local_shard_metadata),
            _sharded_tensor.Shard(torch.randn(5, 5, device=f"cuda:{self.rank}"), local_shard_metadata)
        ]
        with self.assertRaisesRegex(RuntimeError, 'does not match number of local shards metadata'):
            sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
                wrong_num_shards,
                sharded_tensor_metadata
            )

        wrong_size_shards = [_sharded_tensor.Shard(torch.randn(2, 3, device=f"cuda:{self.rank}"), local_shard_metadata)]
        with self.assertRaisesRegex(ValueError, 'Local shard tensor is incompatible with'):
            sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
                wrong_size_shards,
                sharded_tensor_metadata
            )

        wrong_device_shards = [_sharded_tensor.Shard(torch.randn(5, 5), local_shard_metadata)]
        with self.assertRaisesRegex(ValueError, 'Local shard tensor device does not match with local Shard placement'):
            sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
                wrong_device_shards,
                sharded_tensor_metadata
            )

        wrong_dtype_shards = [
            _sharded_tensor.Shard(torch.ones(5, 5, device=f"cuda:{self.rank}", dtype=torch.int), local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, 'Local shard tensor dtype does not match with sharded_tensor_metadata'):
            sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
                wrong_dtype_shards,
                sharded_tensor_metadata
            )

        indices = [[0, 1, 1], [2, 0, 2]]
        values = [3.2, 4.5, 5.8]
        sparse_tensor = torch.sparse_coo_tensor(indices, values, (5, 5), device=f"cuda:{self.rank}")

        wrong_layout_shards = [
            _sharded_tensor.Shard(sparse_tensor, local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, 'Local shard tensor layout does not match with sharded_tensor_metadata'):
            sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
                wrong_layout_shards,
                sharded_tensor_metadata
            )

        wrong_requires_grad_shards = [
            _sharded_tensor.Shard(torch.randn(5, 5, device=f"cuda:{self.rank}", requires_grad=True), local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, 'Local shard tensor requires_grad does not match with sharded_tensor_metadata'):
            sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
                wrong_requires_grad_shards,
                sharded_tensor_metadata
            )

        wrong_pin_memory_shards = [
            _sharded_tensor.Shard(torch.randn(5, 5, pin_memory=True), local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, 'Local shard tensor pin_memory does not match with sharded_tensor_metadata'):
            sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
                wrong_pin_memory_shards,
                sharded_tensor_metadata
            )

        wrong_memory_format_shards = [
            _sharded_tensor.Shard(torch.randn(5, 5, device=f"cuda:{self.rank}").t(), local_shard_metadata)
        ]
        with self.assertRaisesRegex(ValueError, 'Only torch.contiguous_format memory_format is currently supported'):
            sharded_tensor = ShardedTensor._init_from_local_shards_and_global_metadata(
                wrong_memory_format_shards,
                sharded_tensor_metadata
            )

if __name__ == '__main__':
    run_tests()
