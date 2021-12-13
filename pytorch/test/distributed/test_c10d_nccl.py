# Owner(s): ["oncall: distributed"]

import copy
import math
import os
import random
import signal
import sys
import tempfile
import threading
import time
from contextlib import contextmanager
from datetime import timedelta
from itertools import product
from unittest import mock

import torch
import torch.distributed as c10d

if not c10d.is_available():
    print("c10d not available, skipping tests", file=sys.stderr)
    sys.exit(0)

import test_c10d_common
import torch.distributed as dist
import torch.distributed.algorithms.ddp_comm_hooks.default_hooks as default
import torch.distributed.algorithms.ddp_comm_hooks.powerSGD_hook as powerSGD
import torch.nn.functional as F
import torch.testing._internal.common_utils as common
from test_c10d_common import gpus_for_rank, DoubleGpuNet, ConvNet, ModuleForDdpCommHook
from torch import nn
from torch.nn.parallel import DistributedDataParallel
from torch.testing._internal.common_distributed import (
    MultiProcessTestCase,
    init_multigpu_helper,
    requires_nccl,
    requires_gloo,
    requires_nccl_version,
    skip_if_lt_x_gpu,
    get_timeout,
    skip_if_rocm,
    with_dist_debug_levels,
    with_nccl_blocking_wait,
)
from torch.testing._internal.common_utils import (
    TestCase,
    run_tests,
    retry_on_connect_failures,
    TEST_WITH_DEV_DBG_ASAN,
    TEST_WITH_ROCM,
    sandcastle_skip,
    sandcastle_skip_if,
)
from torch.utils.checkpoint import checkpoint
from torch.distributed.optim import functional_optim_map

from torch.distributed.optim.functional_sgd import _FunctionalSGD
from torch.distributed.optim.functional_adam import _FunctionalAdam
from torch.distributed.optim.functional_adamw import _FunctionalAdamW

if TEST_WITH_DEV_DBG_ASAN:
    print(
        "Skip ASAN as torch + multiprocessing spawn have known issues", file=sys.stderr
    )
    sys.exit(0)

# bfloat16 is only supported by CUDA 11+
BFLOAT16_AVAILABLE = (
    torch.cuda.is_available()
    and torch.version.cuda is not None
    and int(torch.version.cuda.split('.')[0]) >= 11)

class RendezvousEnvTest(TestCase):
    @retry_on_connect_failures
    @requires_nccl()
    @sandcastle_skip_if(
        torch.cuda.device_count() == 0, "No GPUs available, skipping test"
    )
    def test_common_errors(self):
        vars = {
            "WORLD_SIZE": "1",
            "RANK": "0",
            "MASTER_ADDR": "127.0.0.1",
            "MASTER_PORT": str(common.find_free_port()),
        }

        class Env(object):
            def __init__(self, vars):
                self.env_patcher = mock.patch.dict(os.environ, vars, clear=True)

            def __enter__(self):
                self.env_patcher.start()

            def __exit__(self, type, value, traceback):
                self.env_patcher.stop()

        def without(d, key):
            d = d.copy()
            d.pop(key)
            return d

        def withouts(d, keys):
            d = d.copy()
            for key in keys:
                d.pop(key)
            return d

        with Env(without(vars, "WORLD_SIZE")):
            self.assertEqual(None, os.environ.get("WORLD_SIZE"))
            with self.assertRaisesRegex(ValueError, "WORLD_SIZE expected"):
                gen = c10d.rendezvous("env://")
                next(gen)
            c10d.init_process_group(backend="nccl", world_size=1)
            self.assertEqual(c10d.get_rank(), 0)
            self.assertEqual(c10d.get_world_size(), 1)
            c10d.destroy_process_group()

        with Env(without(vars, "RANK")):
            self.assertEqual(None, os.environ.get("RANK"))
            with self.assertRaisesRegex(ValueError, "RANK expected"):
                gen = c10d.rendezvous("env://")
                next(gen)
            c10d.init_process_group(backend="nccl", rank=0)
            self.assertEqual(c10d.get_rank(), 0)
            self.assertEqual(c10d.get_world_size(), 1)
            c10d.destroy_process_group()

        with Env(withouts(vars, ["RANK", "WORLD_SIZE"])):
            self.assertEqual(None, os.environ.get("RANK"))
            self.assertEqual(None, os.environ.get("WORLD_SIZE"))
            c10d.init_process_group(backend="nccl", rank=0, world_size=1)
            self.assertEqual(c10d.get_rank(), 0)
            self.assertEqual(c10d.get_world_size(), 1)
            c10d.destroy_process_group()

        with Env(vars):
            c10d.init_process_group(backend="nccl")
            self.assertEqual(c10d.get_rank(), 0)
            self.assertEqual(c10d.get_world_size(), 1)
            c10d.destroy_process_group()

        with Env(without(vars, "MASTER_ADDR")):
            self.assertEqual(None, os.environ.get("MASTER_ADDR"))
            with self.assertRaisesRegex(ValueError, "MASTER_ADDR expected"):
                gen = c10d.rendezvous("env://")
                next(gen)

        with Env(without(vars, "MASTER_PORT")):
            self.assertEqual(None, os.environ.get("MASTER_PORT"))
            with self.assertRaisesRegex(ValueError, "MASTER_PORT expected"):
                gen = c10d.rendezvous("env://")
                next(gen)

        with Env(without(vars, "WORLD_SIZE")):
            self.assertEqual(None, os.environ.get("WORLD_SIZE"))
            gen = c10d.rendezvous("env://?world_size={}".format(1))
            _, _, size = next(gen)
            self.assertEqual(size, 1)

        with Env(without(vars, "RANK")):
            self.assertEqual(None, os.environ.get("RANK"))
            gen = c10d.rendezvous("env://?rank={}".format(0))
            _, rank, _ = next(gen)
            self.assertEqual(rank, 0)

        with Env(withouts(vars, ["RANK", "WORLD_SIZE"])):
            self.assertEqual(None, os.environ.get("RANK"))
            self.assertEqual(None, os.environ.get("WORLD_SIZE"))
            gen = c10d.rendezvous("env://?rank={}&world_size={}".format(0, 1))
            _, rank, size = next(gen)
            self.assertEqual(rank, 0)
            self.assertEqual(size, 1)


class TimeoutTest(test_c10d_common.AbstractTimeoutTest, TestCase):
    @requires_nccl()
    @retry_on_connect_failures
    @sandcastle_skip_if(
        torch.cuda.device_count() == 0, "No GPUs available, skipping test"
    )
    def test_default_store_timeout_nccl(self):
        self._test_default_store_timeout("nccl")


class ProcessGroupNCCLNoGPUTest(TestCase):
    MAIN_PROCESS_RANK = 0

    def setUp(self):
        self.rank = self.MAIN_PROCESS_RANK
        self.world_size = 1
        self.file = tempfile.NamedTemporaryFile(delete=False)

    def tearDown(self):
        pass

    @requires_nccl()
    @sandcastle_skip_if(
        torch.cuda.device_count() > 0, "GPUs are available, skipping test"
    )
    def test_init_no_gpus(self):
        store = c10d.FileStore(self.file.name, self.world_size)
        with self.assertRaisesRegex(
            RuntimeError, "ProcessGroupNCCL is only supported with GPUs, no GPUs found!"
        ):
            c10d.ProcessGroupNCCL(store, self.rank, self.world_size)


class ProcessGroupNCCLTest(MultiProcessTestCase):
    def _create_process_group_nccl(self, store, opts):
        # create nccl processgroup with opts
        c10d.init_process_group(
            "nccl",
            world_size=self.world_size,
            rank=self.rank,
            store=store,
            pg_options=opts)
        pg = c10d.distributed_c10d._get_default_group()
        return pg

    def opts(self, high_priority_stream=False):
        opts = c10d.ProcessGroupNCCL.Options()
        opts.is_high_priority_stream = high_priority_stream
        return opts

    def setUp(self):
        super(ProcessGroupNCCLTest, self).setUp()
        # NCCL_BLOCKING_WAIT overrides NCCL_ASYNC_ERROR_HANDLING hence tests
        # that use NCCL_BLOCKING_WAIT will test it as expected.
        os.environ["NCCL_ASYNC_ERROR_HANDLING"] = "1"
        # self.num_gpus = torch.cuda.device_count()
        self._spawn_processes()

    def tearDown(self):
        super(ProcessGroupNCCLTest, self).tearDown()
        try:
            os.remove(self.file_name)
        except OSError:
            pass

    @property
    def world_size(self):
        return 2

    @property
    def rank_to_GPU(self):
        # return rank to GPU map
        return init_multigpu_helper(self.world_size, "nccl")

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_empty_tensors(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_idx = self.rank_to_GPU[self.rank][0]

        xs = [torch.FloatTensor([]).cuda(local_device_idx)]
        pg.broadcast(xs).wait()
        self.assertEqual(0, xs[0].numel())

        pg.allreduce(xs).wait()
        self.assertEqual(0, xs[0].numel())

        pg.reduce(xs).wait()
        self.assertEqual(0, xs[0].numel())

        ys = [[torch.FloatTensor([]).cuda(local_device_idx) for _ in range(self.world_size)]]
        pg.allgather(ys, xs).wait()
        for y in ys[0]:
            self.assertEqual(0, y.numel())

        ys = [torch.FloatTensor([]).cuda(local_device_idx)]
        xs = [[torch.FloatTensor([]).cuda(local_device_idx) for _ in range(self.world_size)]]
        pg.reduce_scatter(ys, xs).wait()
        self.assertEqual(0, ys[0].numel())

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_broadcast_ops(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())

        def broadcast(xs, rootRank, rootTensor):
            opts = c10d.BroadcastOptions()
            opts.rootRank = rootRank
            opts.rootTensor = rootTensor
            work = pg.broadcast(xs, opts)
            work.wait()
            return work.result()

        # Every rank is root once
        for i in range(self.world_size):
            # Run with 1 input tensor
            x = torch.tensor([self.rank]).cuda(self.rank_to_GPU[self.rank][0])
            output = broadcast([x], i, 0)
            # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
            self.assertEqualIgnoreType(torch.tensor([i]), output[0])

            expected_tensor = torch.empty([i + 1, i + 1]).fill_(i + 1)
            xs = [torch.empty([i + 1, i + 1]).fill_(-1).cuda(device=device_idx) for device_idx in self.rank_to_GPU[self.rank]]

            # test with multiple input tensors (multiple gpu in one rank)
            for j in range(len(xs)):
                if self.rank == i:
                    xs[j] = expected_tensor.cuda(device=self.rank_to_GPU[self.rank][j])

                broadcast(xs, i, j)

                for tensor in xs:
                    self.assertEqual(tensor, expected_tensor)


    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_allreduce_ops(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        device_count = torch.cuda.device_count()
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_id = self.rank_to_GPU[self.rank][0]

        def allreduce(tensors, op):
            opts = c10d.AllreduceOptions()
            opts.reduceOp = op
            work = pg.allreduce(tensors, opts)
            work.wait()

        # Sum
        tensors = [torch.tensor([self.rank + 1]).cuda(local_device_id)]

        allreduce(tensors, c10d.ReduceOp.SUM)

        ndev = float(self.world_size)
        # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
        self.assertEqualIgnoreType(
            torch.tensor([ndev * (ndev + 1) / 2]),
            tensors[0],
        )

        # Avg (only available for NCCL 2.10+)
        if torch.cuda.nccl.version() >= (2, 10, 0):
            tensors = [torch.tensor([self.rank + 1.]).cuda(local_device_id)]

            allreduce(tensors, c10d.ReduceOp.AVG)

            # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
            self.assertEqualIgnoreType(
                torch.tensor([ndev * (ndev + 1.) / (2. * ndev)]),
                tensors[0],
            )

        # Product
        tensors = [torch.tensor([self.rank + 1]).cuda(local_device_id)]

        allreduce(tensors, c10d.ReduceOp.PRODUCT)
        # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
        self.assertEqualIgnoreType(
            torch.tensor([float(math.factorial(self.world_size))]), tensors[0]
        )

        # Min
        tensors = [torch.tensor([self.rank + 1]).cuda(local_device_id)]

        allreduce(tensors, c10d.ReduceOp.MIN)
        # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
        self.assertEqualIgnoreType(torch.tensor([1.0]), tensors[0])

        # Max
        tensors = [torch.tensor([self.rank + 1]).cuda(local_device_id)]

        allreduce(tensors, c10d.ReduceOp.MAX)
        self.assertEqual(torch.tensor([self.world_size]), tensors[0])

        for op in (c10d.ReduceOp.BAND, c10d.ReduceOp.BOR, c10d.ReduceOp.BXOR):
            with self.assertRaisesRegex(
                RuntimeError, "Cannot use " + str(op) + " with NCCL"
            ):
                allreduce(tensors, op)

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_reduce_ops(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_id = self.rank_to_GPU[self.rank][0]

        def reduce(xs, rootRank, rootTensor, op=None):
            opts = c10d.ReduceOptions()
            opts.rootRank = rootRank
            opts.rootTensor = rootTensor
            if op:
                opts.reduceOp = op
            work = pg.reduce(xs, opts)
            work.wait()

        # for every root tensor
        for rt in range(self.world_size):
            tensors = [torch.tensor([self.rank + 1]).cuda(local_device_id)]

            reduce(tensors, rt, 0)

            # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
            if self.rank == rt:
                self.assertEqualIgnoreType(
                    torch.tensor([float(self.world_size * (self.world_size + 1) / 2)]),
                    tensors[0],
                )
            else:
                self.assertEqualIgnoreType(
                    torch.tensor([self.rank + 1]),
                    tensors[0],
                )


            for op in (c10d.ReduceOp.BAND, c10d.ReduceOp.BOR, c10d.ReduceOp.BXOR):
                with self.assertRaisesRegex(
                    RuntimeError, "Cannot use " + str(op) + " with NCCL"
                ):
                    reduce(tensors, self.rank, rt, op)

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_allgather_ops(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_ids = self.rank_to_GPU[self.rank]

        def allgather(output_ts, input_ts):
            work = pg.allgather(output_ts, input_ts)
            work.wait()

        tensors = [torch.empty(2, 2).fill_(2).cuda(device=i) for i in local_device_ids]
        output_tensors = []
        expected_output = []

        output_per_gpu = ([torch.empty(2, 2).fill_(-1)] * len(local_device_ids) * self.world_size)
        expected_per_gpu = ([torch.empty(2, 2).fill_(2)] * len(local_device_ids) * self.world_size)

        for gpu in local_device_ids:
            output_tensors.append([t.cuda(device=gpu) for t in output_per_gpu])
            expected_output.append([t.cuda(device=gpu) for t in expected_per_gpu])

        allgather(output_tensors, tensors)

        # Verification
        self.assertEqual(output_tensors, expected_output)

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_allgather_base_ops(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_id = self.rank_to_GPU[self.rank][0]

        def allgather_base(output_t, input_t):
            work = pg._allgather_base(output_t, input_t)
            work.wait()

        # allgather_base is GPU number agnostic.
        # Each rank contribute one tensor regardless of GPU counts
        tensor = torch.tensor([self.rank]).cuda(local_device_id)
        output_t = torch.empty((self.world_size), dtype=tensor.dtype).cuda(local_device_id)

        allgather_base(output_t, tensor)

        # Verification
        self.assertEqual(torch.arange(self.world_size), output_t)

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_allgather_base_basics(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_id = self.rank_to_GPU[self.rank][0]

        def allgather_base(output_t, input_t):
            work = pg._allgather_base(output_t, input_t)
            work.wait()

        # anticpate an error
        with self.assertRaisesRegex(
            RuntimeError,
            "output tensor size must be equal to world_size times input tensor size",
        ):
            tensor = torch.tensor([self.rank]).cuda(local_device_id)
            output_t = torch.empty((self.world_size + 1), dtype=tensor.dtype).cuda(
                local_device_id
            )
            # fails the check because output_t is not correctly sized
            allgather_base(output_t, tensor)

        # anticpate an error
        with self.assertRaisesRegex(
            RuntimeError, "output tensor must have the same type as input tensor"
        ):
            tensor = torch.tensor([self.rank], dtype=torch.float).cuda(local_device_id)
            output_t = torch.empty((self.world_size + 1), dtype=torch.long).cuda(
                local_device_id
            )
            # fails the check because the dtype is different
            allgather_base(output_t, tensor)

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_reduce_scatter_base_basics(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_id = self.rank_to_GPU[self.rank][0]

        def reduce_scatter_base(output_t, input_t):
            work = pg._reduce_scatter_base(output_t, input_t)
            work.wait()

        # anticpate an error
        with self.assertRaisesRegex(
            RuntimeError,
            "input tensor must be the same size as output size times world size",
        ):
            input_t = torch.tensor([self.rank]).cuda(local_device_id)
            output_t = torch.empty((self.world_size + 1), dtype=input_t.dtype).cuda(
                local_device_id
            )
            # fails the check because output_t is not correctly sized
            reduce_scatter_base(output_t, input_t)

        # anticpate an error
        with self.assertRaisesRegex(
            RuntimeError, "input tensor must be the same type as the outut tensor."
        ):
            tensor = torch.tensor([self.rank], dtype=torch.float).cuda(local_device_id)
            output_t = torch.empty((self.world_size + 1), dtype=torch.long).cuda(
                local_device_id
            )
            # fails the check because the dtype is different
            reduce_scatter_base(output_t, tensor)

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_reduce_scatter_ops(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_ids = self.rank_to_GPU[self.rank]
        num_gpus = len(local_device_ids)

        def reduce_scatter(outputs, input_lists, op):
            opts = c10d.ReduceScatterOptions()
            opts.reduceOp = op
            work = pg.reduce_scatter(outputs, input_lists, opts)
            work.wait()

        output = [torch.tensor([0]).cuda(i) for i in local_device_ids]

        #  GPU/rank
        #   0         [1], [2], [3], [4]
        #   1         [2], [3], [4], [5]
        #   2         [3], [4], [5], [6]
        #   3         [4], [5], [6], [7]

        # Sum
        tensor_lists = []
        input_per_gpu = []

        for i in range(self.world_size):
            input_per_gpu.append(torch.tensor([self.rank + i + 1]))

        for gpu in local_device_ids:
            tensor_lists.append([t.cuda(device=gpu) for t in input_per_gpu])

        reduce_scatter(output, tensor_lists, c10d.ReduceOp.SUM)

        for i in range(num_gpus):
            expected = torch.tensor(
                [
                    float((1 + self.world_size) * self.world_size / 2)
                    + self.world_size * self.rank
                ])


            # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
            self.assertEqualIgnoreType(expected, output[i])

        # Min
        reduce_scatter(output, tensor_lists, c10d.ReduceOp.MIN)

        for i in range(num_gpus):
            expected = torch.tensor([self.rank + 1 + i])
            self.assertEqual(expected, output[i])

        # Max
        reduce_scatter(output, tensor_lists, c10d.ReduceOp.MAX)

        for i in range(num_gpus):
            expected = torch.tensor(
                [self.rank + self.world_size + i]
            )
            self.assertEqual(expected, output[i])

        # Product
        reduce_scatter(output, tensor_lists, c10d.ReduceOp.PRODUCT)

        # math pakcage don't have math.perm until python 3.8, so
        # we implement a naive version here.
        def perm(n, k):
            prod_val = n
            for val in range(n - k + 1, n):
                prod_val *= val
            return prod_val

        for i in range(num_gpus):
            prod_val = perm(self.rank + self.world_size, self.world_size)

            expected = torch.tensor([prod_val])
            # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
            self.assertEqualIgnoreType(expected, output[i])

        # Test the input params overridden scenarios, aka, when the input is
        # a list and output is just one tensor.
        # Sum
        output_tensor = torch.empty_like(input_per_gpu[0][0]).cuda(self.rank)
        input_list = [tensor[0].cuda(self.rank) for tensor in input_per_gpu]
        pg.reduce_scatter(output_tensor, input_list, c10d.ReduceOp.SUM).wait()
        expected = torch.tensor(
            float((1 + self.world_size) * self.world_size / 2) + self.world_size * self.rank
        )
        self.assertEqualIgnoreType(expected, output_tensor)

        # Min
        pg.reduce_scatter(output_tensor, input_list, c10d.ReduceOp.MIN).wait()
        expected = torch.tensor(self.rank + 1)
        self.assertEqualIgnoreType(expected, output_tensor)

        # Max
        pg.reduce_scatter(output_tensor, input_list, c10d.ReduceOp.MAX).wait()
        expected = torch.tensor(self.rank + self.world_size)
        self.assertEqualIgnoreType(expected, output_tensor)

        # Product
        pg.reduce_scatter(output_tensor, input_list, c10d.ReduceOp.PRODUCT).wait()
        prod_val = self.rank + 1
        for k in range(1, self.world_size):
            prod_val = prod_val * (self.rank + 1 + k)
        expected = torch.tensor(prod_val)
        self.assertEqualIgnoreType(expected, output_tensor)

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_reduce_scatter_base_ops(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_id = self.rank_to_GPU[self.rank][0]

        def reduce_scatter_base(output_t, input_t):
            work = pg._reduce_scatter_base(output_t, input_t)
            work.wait()

        # reduce_scatter_base is GPU number agnostic.
        # Each rank contribute one tensor regardless of GPU counts
        output_t = torch.empty([1]).cuda(local_device_id)
        tensor = torch.arange(self.world_size, dtype=output_t.dtype).cuda(local_device_id)

        reduce_scatter_base(output_t, tensor)

        # Verification
        self.assertEqual(output_t[0], self.rank * self.world_size)

    @requires_nccl()
    @sandcastle_skip_if(torch.cuda.device_count() < 2, "NCCL test requires 2+ GPUs")
    def test_barrier(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        pg = self._create_process_group_nccl(store, self.opts())
        local_device_ids = self.rank_to_GPU[self.rank]

        def allreduce(tensors):
            opts = c10d.AllreduceOptions()
            work = pg.allreduce(tensors, opts)
            return work

        # Making the collective to operate on
        # 1, 2, 3, 4, .... len(local_device_ids) GPUs
        tensors_list = [[] for _ in range(len(local_device_ids))]

        for i in range(1, len(local_device_ids) + 1):
            for j in range(i):
                tensors_list[i - 1].append(torch.tensor([j + 1]).cuda(local_device_ids[j]))

        works = []
        for tensors in tensors_list:
            work = allreduce(tensors)
            works.append(work)

        # Barrier will ensure that all previous work is completed
        pg.barrier().wait()

        for i in range(1, len(local_device_ids) + 1):
            for j in range(i):
                # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
                self.assertEqualIgnoreType(
                    torch.tensor([(j + 1) * self.world_size]), tensors_list[i - 1][j]
                )


class DistributedDataParallelTest(
    test_c10d_common.AbstractDistributedDataParallelTest, MultiProcessTestCase
):
    def setUp(self):
        super(DistributedDataParallelTest, self).setUp()
        # NCCL_BLOCKING_WAIT overrides NCCL_ASYNC_ERROR_HANDLING hence tests
        # that use NCCL_BLOCKING_WAIT will test it as expected.
        os.environ["NCCL_ASYNC_ERROR_HANDLING"] = "1"
        self._spawn_processes()

    def _test_nccl_backend(
        self, devices, device_ids, multi_device=False, gradient_as_bucket_view=False
    ):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        self._test_ddp_with_process_group(
            process_group, devices, device_ids, multi_device, gradient_as_bucket_view
        )

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_nccl_propagate_error_reason(self):
        # Need to use NCCL_BLOCKING_WAIT and not ASYNC_ERROR_HANDLING,
        # otherwise process will be taken down and we can't check for errors.
        os.environ["NCCL_ASYNC_ERROR_HANDLING"] = "0"
        os.environ["NCCL_BLOCKING_WAIT"] = "1"
        # TODO: smaller timeout can fail since PG NCCl does health check in
        # constructor. Look into reducing this test's runtime.
        store = c10d.FileStore(self.file_name, self.world_size)
        # provide sufficient timeout to initialize NCCL comm.
        pg = c10d.ProcessGroupNCCL(store, self.rank, self.world_size, timeout=timedelta(seconds=15))
        pg_gloo = c10d.ProcessGroupGloo(store, self.rank, self.world_size)
        pg.barrier().wait(timedelta(seconds=5))
        # Simulate stuckness in rank 0.
        if self.rank == 0:
            pg_gloo.barrier().wait()
        inp = torch.ones(1).cuda(self.rank)

        if self.rank != 0:
            # Time out due to rank 0 not calling into allreduce.
            with self.assertRaises(RuntimeError):
                pg.allreduce([inp]).wait(timedelta(seconds=5))

            # Now when nonzero rank attempts to use communicator, original failure reason should be logged.j
            try:
                pg.allreduce([torch.ones(2).cuda(self.rank)]).wait()
            except RuntimeError as e:
                self.assertTrue("timed out in call to wait()" in str(e))
                self.assertTrue("TensorShape=[1]" in str(e))
            else:
                self.fail("Expected error to be raised!")

            # Unblock rank 0
            pg_gloo.barrier().wait()

        # TODO: We can also test that if rank 0 attempts to use the communicator,
        # then we should error out with the info that it was aborted due to
        # timeout on another rank. Although this would only be the case after
        # the watchdog has run on the rank, and there is no reliable way
        # to confirm it has run.

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_nccl_backend_multi_device_ids_not_allowed(self):
        int_devices = list(range(torch.cuda.device_count()))
        devices = [torch.device("cuda:" + str(i)) for i in int_devices]
        with self.assertRaisesRegex(
            ValueError, "device_ids can only be None or contain a single element."
        ):
            self._test_nccl_backend(devices, int_devices)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_nccl_backend_single_device_module_device_ids_None(self):
        self._test_nccl_backend(None, None)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_nccl_backend_single_device_module_empty_device_ids(self):
        # This tests the backward compatibility of accepting an empty list as `device_ids`,
        # although we no longer document this in favor of the default value of `None`,
        # which is consistent with multi-device modules and CPU modules.
        self._test_nccl_backend(None, [])

    @requires_nccl()
    @skip_if_lt_x_gpu(4)
    def test_nccl_backend_multi_device_module_device_ids_None(self):
        int_devices = gpus_for_rank(self.world_size)[self.rank][:2]
        devices = [torch.device("cuda:" + str(i)) for i in int_devices]
        self._test_nccl_backend(devices, None, multi_device=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_nccl_backend_1gpu_module_device_ids_integer_list(self):
        int_devices = gpus_for_rank(self.world_size)[self.rank][:1]
        devices = [torch.device("cuda:" + str(i)) for i in int_devices]
        self._test_nccl_backend(devices, int_devices)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_nccl_backend_1gpu_module_device_ids_torch_device_list(self):
        int_devices = gpus_for_rank(self.world_size)[self.rank][:1]
        devices = [torch.device("cuda:" + str(i)) for i in int_devices]
        self._test_nccl_backend(devices, devices)

    @requires_nccl()
    @skip_if_lt_x_gpu(4)
    def test_nccl_backend_2gpu_module(self):
        int_devices = gpus_for_rank(self.world_size)[self.rank][:2]
        devices = [torch.device("cuda:" + str(i)) for i in int_devices]
        self._test_nccl_backend(devices, None, multi_device=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(8)
    def test_nccl_backend_4gpu_module(self):
        int_devices = gpus_for_rank(self.world_size)[self.rank][:4]
        devices = [torch.device("cuda:" + str(i)) for i in int_devices]
        self._test_nccl_backend(devices, None, multi_device=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(4)
    def test_ddp_multi_device_module_config(self):
        gpus = gpus_for_rank(self.world_size)[self.rank]

        self.assertTrue(len(gpus) >= 2, "expecting at least 2 gpus per process")

        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        gpus = gpus[:2]
        model = DoubleGpuNet(gpus)

        with self.assertRaisesRegex(
            ValueError,
            "DistributedDataParallel device_ids and output_device arguments only work with "
            "single-device/multiple-device GPU modules or CPU modules",
        ):
            ddp_model = DistributedDataParallel(
                model, output_device=gpus[1], process_group=process_group
            )

        with self.assertRaisesRegex(
            ValueError, "device_ids can only be None or contain a single element."
        ):
            ddp_model = DistributedDataParallel(
                model, device_ids=gpus, process_group=process_group
            )

        with self.assertRaisesRegex(
            ValueError, "input module must be on the same type of devices"
        ):
            model.fc1 = model.fc1.cpu()
            ddp_model = DistributedDataParallel(model, process_group=process_group)

        model = model.cpu()
        with self.assertRaisesRegex(
            ValueError, "device_ids can only be None or contain a single element."
        ):
            ddp_model = DistributedDataParallel(
                model, device_ids=gpus, process_group=process_group
            )

    def _test_fp16(self, gradient_as_bucket_view=False):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        gpus = gpus_for_rank(self.world_size)[self.rank]
        model = nn.Linear(1, 1, bias=False).cuda(gpus[0]).half()
        nn.init.constant_(model.weight, 1)
        ddp_model = DistributedDataParallel(
            model,
            device_ids=[gpus[0]],
            process_group=process_group,
            bucket_cap_mb=0.001,
            gradient_as_bucket_view=gradient_as_bucket_view,
        )

        # Input 2**15, so that the gradients will overflow with a
        # world_size of 2, unless we normalize the gradient by the
        # world_size before the reduction
        input = torch.tensor([[2 ** 15]]).cuda(gpus[0]).half()

        # Step model
        ddp_model.train()
        output = ddp_model(input)
        loss = output.sum()
        loss.backward()

        self.assertFalse(any(torch.isinf(p.grad).any() for p in ddp_model.parameters()))

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_fp16(self):
        self._test_fp16()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_fp16_grad_is_view(self):
        self._test_fp16(gradient_as_bucket_view=True)

    def _test_arbitrary_forward_return_value(self, gradient_as_bucket_view=False):
        """
        Note: this test can be sped up by only running it on a CPU module
        once DistributedDataParallel supports them.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        class ForwardReturnValueModule(nn.Module):
            def __init__(self):
                super(ForwardReturnValueModule, self).__init__()
                self.fc1 = nn.Linear(2, 10, bias=False)
                self.fc2 = nn.Linear(10, 4, bias=False)
                self.fc3 = nn.Linear(4, 4, bias=False)
                self.relu = nn.ReLU()

            def forward(self, x, fn):
                x = self.relu(self.fc1(x))
                x = self.relu(self.fc2(x))
                # The first softmax does NOT include fc3 in its autograd graph
                # whereas the second softmax DOES. If we pass only the first
                # tensor we see in the output to the reducer, it marks the
                # gradient for fc3 as ready (because it doesn't show up). If
                # downstream uses of this return value choose to differentiate
                # against the second output tensor, it would still receive a
                # gradient and a callback for this tensor, resulting in a crash.
                return fn(
                    F.softmax(x, dim=1),
                    F.softmax(self.fc3(x), dim=1),
                )

        device_id = gpus_for_rank(self.world_size)[self.rank][0]
        model = DistributedDataParallel(
            ForwardReturnValueModule().float().to(device_id),
            device_ids=[device_id],
            process_group=process_group,
            gradient_as_bucket_view=gradient_as_bucket_view,
        )

        batch_size = 4
        criterion = nn.CrossEntropyLoss()
        input = torch.rand([batch_size, 2], dtype=torch.float)
        target = torch.LongTensor([random.randrange(4) for _ in range(batch_size)]).to(
            device_id
        )

        # Always run "backward" to ensure the reducer is called by autograd.
        # If we don't correctly capture the output tensors from the return value,
        # the reducer won't see a hook for the unused parameter, and throw an error.
        # The correct capture is what we're testing in this function.
        def test(box, unbox):
            output = model(input, fn=box)
            loss = criterion(unbox(output), target)
            loss.backward()

        # Test with identity return value
        test(
            box=lambda x, y: (x, y),
            unbox=lambda obj: obj[1],
        )

        # Test with list return value
        test(
            box=lambda x, y: ["foo", x, "bar", y],
            unbox=lambda obj: obj[3],
        )

        # Test with tuple return value
        test(
            box=lambda x, y: ("foo", x, "bar", y),
            unbox=lambda obj: obj[3],
        )

        # Test with dict return value
        test(
            box=lambda x, y: {"foo": "bar", "a": x, "b": y},
            unbox=lambda obj: obj["b"],
        )

        # Test with list with dict return value
        test(
            box=lambda x, y: ["foo", "bar", {"a": x, "b": y}],
            unbox=lambda obj: obj[2]["b"],
        )

        # Test with dict with list return value
        test(
            box=lambda x, y: {"foo": "bar", "list": [0, x, 1, y]},
            unbox=lambda obj: obj["list"][3],
        )

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_arbitrary_forward_return_value(self):
        self._test_arbitrary_forward_return_value()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_arbitrary_forward_return_value_grad_is_view(self):
        self._test_arbitrary_forward_return_value(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_with_lazy_parameters(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        with self.assertRaisesRegex(
            RuntimeError, "Modules with uninitialized parameters"
        ):
            DistributedDataParallel(
                torch.nn.LazyLinear(10), process_group=process_group
            )

    def _test_find_unused_parameters_kwarg(self, gradient_as_bucket_view=False):
        """
        Note: this test can be sped up by only running it on a CPU module
        once DistributedDataParallel supports them.
        """
        torch.cuda.set_device(self.rank)
        dist.init_process_group(
            backend="nccl",
            world_size=self.world_size,
            rank=self.rank,
            init_method=f"file://{self.file_name}",
        )
        process_group = c10d.distributed_c10d._get_default_group()

        class FindUnusedParametersModule(nn.Module):
            def __init__(self):
                super(FindUnusedParametersModule, self).__init__()
                self.fc1 = nn.Linear(2, 10, bias=False)
                self.fc2 = nn.Linear(10, 4, bias=False)
                self.fc3 = nn.Linear(4, 4, bias=False)
                self.relu = nn.ReLU()

            def forward(self, x):
                x = self.relu(self.fc1(x))
                x = self.relu(self.fc2(x))
                # Return the fc3 module so that the caller can invoke it
                # outside of the forward function. While this is bad practice,
                # we can use it to trigger a reducer error.
                return (F.softmax(x, dim=1), self.fc3)

        device_id = gpus_for_rank(self.world_size)[self.rank][0]
        batch_size = 4
        criterion = nn.CrossEntropyLoss()
        input = torch.rand([batch_size, 2], dtype=torch.float)
        target = torch.LongTensor([random.randrange(4) for _ in range(batch_size)]).to(
            device_id
        )

        ddp_model = None

        def test_find_unused_parameters(
            find_unused_parameters, test_default=False, gradient_as_bucket_view=False
        ):
            if test_default:
                model = DistributedDataParallel(
                    FindUnusedParametersModule().float().to(device_id),
                    device_ids=[device_id],
                    process_group=process_group,
                    gradient_as_bucket_view=gradient_as_bucket_view,
                )
            else:
                model = DistributedDataParallel(
                    FindUnusedParametersModule().float().to(device_id),
                    device_ids=[device_id],
                    process_group=process_group,
                    find_unused_parameters=find_unused_parameters,
                    gradient_as_bucket_view=gradient_as_bucket_view,
                )
            nonlocal ddp_model
            ddp_model = model

            output, fc3 = model(input)
            output = fc3(output)
            loss = criterion(output, target)
            loss.backward()

        # First test that finding unused params under these conditions is to
        # trigger an error when `backward` is called (because fc3 is an unused
        # parameter and will therefore be marked ready twice).
        try:
            test_find_unused_parameters(
                True, gradient_as_bucket_view=gradient_as_bucket_view
            )
        except Exception as ex:
            self.assertTrue(
                str(ex).startswith(
                    "Expected to mark a variable ready only once.",
                )
            )
            unused_index = 2
            unused_index_str = f"Parameter at index {unused_index}"
            model = ddp_model.module
            for module_name, module in model.named_modules():
                if module == model.fc3:
                    for parameter_name, _ in module.named_parameters(recurse=False):
                        unused_fqn = f"{module_name}.{parameter_name}"
                        # Only one such parameter in model.fc3, since bias=False
                        break

            if dist._get_debug_mode() != dist._DistributedDebugLevel.OFF:
                unused_index_str += f" with name {unused_fqn}"

            self.assertTrue(unused_index_str in str(ex))
        else:
            self.fail("Expected exception")

        dist.barrier(process_group)

        # Then test that the default behavior can be overridden by setting
        # `find_unused_parameters=False`.
        try:
            test_find_unused_parameters(
                False, gradient_as_bucket_view=gradient_as_bucket_view
            )
        except Exception as ex:
            self.fail("Unexpected exception: %s" % ex)

        # Test find_unused_parameters defaults to False
        try:
            test_find_unused_parameters(
                True, test_default=True, gradient_as_bucket_view=gradient_as_bucket_view
            )
        except Exception as ex:
            self.fail("Unexpected exception: %s" % ex)

    # TODO: Combine the following tests once https://github.com/pytorch/pytorch/issues/55967
    # is resolved.
    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @with_dist_debug_levels(levels=["DETAIL"])
    def test_find_unused_parameters_kwarg_debug_detail(self):
        self._test_find_unused_parameters_kwarg()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @with_dist_debug_levels(levels=["INFO"])
    def test_find_unused_parameters_kwarg_debug_info(self):
        self._test_find_unused_parameters_kwarg()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @with_dist_debug_levels(levels=["OFF"])
    def test_find_unused_parameters_kwarg_debug_off(self):
        self._test_find_unused_parameters_kwarg()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @with_dist_debug_levels(levels=["DETAIL"])
    def test_find_unused_parameters_kwarg_grad_is_view_debug_detail(self):
        self._test_find_unused_parameters_kwarg(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @with_dist_debug_levels(levels=["INFO"])
    def test_find_unused_parameters_kwarg_grad_is_view_debug_info(self):
        self._test_find_unused_parameters_kwarg(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @with_dist_debug_levels(levels=["OFF"])
    def test_find_unused_parameters_kwarg_grad_is_view_debug_off(self):
        self._test_find_unused_parameters_kwarg(gradient_as_bucket_view=True)

    def _test_multiple_outputs_multiple_backward(self, gradient_as_bucket_view=False):
        """
        Note: this test can be sped up by only running it on a CPU module
        once DistributedDataParallel supports them.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        class MultipleOutputModule(nn.Module):
            def __init__(self):
                super(MultipleOutputModule, self).__init__()

                def define_module():
                    return nn.Sequential(
                        nn.Linear(2, 10, bias=False),
                        nn.ReLU(),
                        nn.Linear(10, 4, bias=False),
                        nn.ReLU(),
                    )

                self.module0 = define_module()
                self.module1 = define_module()

            def forward(self, x):
                return (
                    F.softmax(self.module0(x), dim=1),
                    F.softmax(self.module1(x), dim=1),
                )

        device_id = gpus_for_rank(self.world_size)[self.rank][0]
        model = DistributedDataParallel(
            MultipleOutputModule().float().to(device_id),
            device_ids=[device_id],
            process_group=process_group,
            gradient_as_bucket_view=gradient_as_bucket_view,
        )

        batch_size = 4
        criterion = nn.CrossEntropyLoss()
        input = torch.rand([batch_size, 2], dtype=torch.float)
        target = torch.LongTensor([random.randrange(4) for _ in range(batch_size)]).to(
            device_id
        )

        # Compute loss and gradients for both outputs
        output1, output2 = model(input)
        loss1 = criterion(output1, target)
        loss1.backward()
        loss2 = criterion(output2, target)
        loss2.backward()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_multiple_outputs_multiple_backward(self):
        self._test_multiple_outputs_multiple_backward()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_multiple_outputs_multiple_backward_grad_is_view(self):
        self._test_multiple_outputs_multiple_backward(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_no_grad(self):
        """
        Note: this test can be sped up by only running it on a CPU module
        once DistributedDataParallel supports them.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        class NoGradModule(nn.Module):
            def __init__(self):
                super(NoGradModule, self).__init__()
                self.fc1 = nn.Linear(2, 10, bias=False)
                self.fc2 = nn.Linear(10, 4, bias=False)
                self.relu = nn.ReLU()

            def forward(self, x):
                x = self.relu(self.fc1(x))
                x = self.relu(self.fc2(x))
                return F.softmax(x, dim=1)

        device_id = gpus_for_rank(self.world_size)[self.rank][0]
        model = DistributedDataParallel(
            NoGradModule().float().to(device_id),
            device_ids=[device_id],
            process_group=process_group,
        )

        batch_size = 4
        input = torch.rand([batch_size, 2], dtype=torch.float)

        def check_no_grads():
            for p in model.parameters():
                self.assertTrue(p.requires_grad)
                self.assertIsNone(p.grad)

        # After initialization, no parameter has their gradient set.
        check_no_grads()

        # Run `forward` function with torch.no_grad()
        with torch.no_grad():
            output = model(input)
            self.assertTrue(isinstance(output, torch.Tensor))

        # No parameter should have their gradient set.
        check_no_grads()

    def _test_accumulate_gradients_module(self, gradient_as_bucket_view=False):
        # This is NOT the recommended way to implement accumulating grads, but
        # we would like to make sure DDP does not mess up with the underlying
        # module.
        int_devices = gpus_for_rank(self.world_size)[self.rank][:1]
        devices = [torch.device("cuda:" + str(i)) for i in int_devices]
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        global_batch_size = self.world_size

        model, ddp_model, input, target = self._prepare_single_device_module(
            process_group, devices, devices, global_batch_size, gradient_as_bucket_view
        )

        def step_model(model, input, target):
            model.train()
            output = model(input)
            loss = F.mse_loss(output, target.to(output.device))
            loss.backward()

        # ensure accumulate grads works with no_grad
        with torch.no_grad():
            ddp_model.train()
            ddp_model.module(input)

        # Check two model parameters over 4 iterations.
        # Use 4 iterations because we alternate between reducing and
        # not reducing and want to make sure we switch both ways.
        for iteration in range(4):
            step_model(model, input, target)

            if iteration % 2 == 0:
                # Skip gradients sync without calling prepare_for_backward
                step_model(
                    ddp_model.module,
                    input[self.rank : (self.rank + 1)],
                    target[self.rank : (self.rank + 1)],
                )
                for i, j in zip(model.parameters(), ddp_model.parameters()):
                    self.assertNotEqual(i.grad, j.grad)
            else:
                step_model(
                    ddp_model,
                    input[self.rank : (self.rank + 1)],
                    target[self.rank : (self.rank + 1)],
                )
                for i, j in zip(model.parameters(), ddp_model.parameters()):
                    # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
                    self.assertEqualIgnoreType(i.grad, j.grad, rtol=1.3e-06, atol=5e-5)

            # Shuffle the input so that DDP input is different
            torch.manual_seed(1337 + iteration)
            input = input[torch.randperm(global_batch_size)]

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_accumulate_gradients_module(self):
        self._test_accumulate_gradients_module()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_accumulate_gradients_module_with_grad_is_view(self):
        self._test_accumulate_gradients_module(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_failure_recovery(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        # need to create a separate file for the recovered FileStore, because
        # the original one will be deleted when destructing the first FileStore.
        recovery_filename = self.file_name + "_recovery"

        if self.rank == 0:
            # the file will be deleted by the recovered FileStore
            open(recovery_filename, "w").close()

        # not necessary to run barrier here, as DDP will synchronize

        class TestModel(nn.Module):
            def __init__(self):
                super(TestModel, self).__init__()
                self.fc1 = nn.Linear(2, 10, bias=False)
                self.fc2 = nn.Linear(10, 4, bias=False)
                self.relu = nn.ReLU()

            def forward(self, x):
                x = self.relu(self.fc1(x))
                x = self.relu(self.fc2(x))
                return F.softmax(x, dim=1)

        device_id = gpus_for_rank(self.world_size)[self.rank][0]
        model = TestModel().float().to(device_id)
        ddp = DistributedDataParallel(
            model,
            device_ids=[device_id],
            process_group=process_group,
        )

        batch_size = 4
        criterion = nn.CrossEntropyLoss()
        input = torch.rand([batch_size, 2], dtype=torch.float)
        target = torch.LongTensor([random.randrange(4) for _ in range(batch_size)]).to(
            device_id
        )

        for _ in range(6):
            output = ddp(input)
            loss = criterion(output, target)
            loss.backward()

        del ddp
        del process_group
        del store  # this will delete self.file_name

        store = c10d.FileStore(recovery_filename, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        ddp = DistributedDataParallel(
            model,
            device_ids=[device_id],
            process_group=process_group,
        )

        input = torch.rand([batch_size, 2], dtype=torch.float)
        target = torch.LongTensor([random.randrange(4) for _ in range(batch_size)]).to(
            device_id
        )
        for _ in range(6):
            output = ddp(input)
            loss = criterion(output, target)
            loss.backward()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_pass_default_pg(self):
        dist.init_process_group(
            "nccl",
            init_method=f"file://{self.file_name}",
            world_size=self.world_size,
            rank=self.rank,
        )

        default_pg = c10d.distributed_c10d._get_default_group()
        dist.destroy_process_group(default_pg)
        self.assertFalse(dist.is_initialized())

    def _test_grad_layout(self, replica_devices, layer_devs, local_batch_size):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        global_batch_size = local_batch_size * self.world_size

        # Carry out some trials with small buckets and some with big buckets.
        bucketsizes = (0.000001, 25)
        # Tuples of lists.  Each list describes per-layer characteristics for one trial.
        layer_formats = (
            [torch.contiguous_format] * 4,
            [torch.channels_last] * 2 + [torch.contiguous_format] * 2,
            [torch.channels_last] * 4,
        )
        layer_dtypes = (
            [torch.float] * 4,
            [torch.float] * 2 + [torch.half] * 2,
            [torch.half] * 4,
        )

        input_dev = layer_devs[0] if isinstance(layer_devs, list) else layer_devs
        target_dev = layer_devs[-1] if isinstance(layer_devs, list) else layer_devs
        input = torch.randn(
            (global_batch_size, 8, 8, 8), device=input_dev, dtype=torch.float
        )
        target = torch.randn(
            (global_batch_size, 8, 4, 4), device=target_dev, dtype=torch.float
        )
        local_batch_start = self.rank * local_batch_size
        local_batch_end = (self.rank + 1) * local_batch_size

        # Reducer.cpp sneakily creates one "initial bucket" that ignores the "bucket_cap_mb"
        # argument.  The following makes sure the initial bucket also complies.
        @contextmanager
        def first_bucket_size(ddp_bucket_mb):
            old_DEFAULT_FIRST_BUCKET_BYTES = dist._DEFAULT_FIRST_BUCKET_BYTES
            dist._DEFAULT_FIRST_BUCKET_BYTES = int(ddp_bucket_mb * 1.0e6)
            try:
                yield
            finally:
                dist._DEFAULT_FIRST_BUCKET_BYTES = old_DEFAULT_FIRST_BUCKET_BYTES

        with torch.backends.cudnn.flags(
            enabled=True, deterministic=True, benchmark=False
        ):
            for formats, dtypes, bucketsize in product(
                layer_formats, layer_dtypes, bucketsizes
            ):
                with first_bucket_size(bucketsize):
                    model_msg = (
                        "rank = {} formats = {} dtypes = {} bucketsize = {} ".format(
                            self.rank, formats, dtypes, bucketsize
                        )
                    )
                    try:
                        m = ConvNet(layer_devs, formats, dtypes)
                        m_ddp = DistributedDataParallel(
                            copy.deepcopy(m),
                            device_ids=replica_devices,
                            process_group=process_group,
                            bucket_cap_mb=bucketsize,
                        )
                        opt = torch.optim.SGD(m.parameters(), lr=0.1)
                        opt_ddp = torch.optim.SGD(m_ddp.parameters(), lr=0.1)
                        has_half = any(p.dtype is torch.half for p in m.parameters())
                        tol = 1.0e-3 if has_half else 1.0e-5
                    except BaseException:
                        # Prints case-specific debugging info to narrow down failing case.
                        print(
                            "Caught exception during model creation for " + model_msg,
                            flush=True,
                        )
                        raise
                    # 3 iters:  First iter creates grads, second iter retests after rebucketing,
                    # third iter tries zeroed grads.
                    for it in range(3):
                        iter_msg = "iter = {} ".format(it) + model_msg
                        named_msg = iter_msg
                        try:
                            F.mse_loss(m(input).float(), target).backward()
                            F.mse_loss(
                                m_ddp(input[local_batch_start:local_batch_end]).float(),
                                target[local_batch_start:local_batch_end],
                            ).backward()
                            for i, ((layer_name, m_child), m_ddp_child) in enumerate(
                                zip(m.named_children(), m_ddp.module.children())
                            ):
                                named_msg = layer_name + ".weight" + " " + iter_msg
                                self.assertTrue(
                                    m_child.weight.grad.is_contiguous(
                                        memory_format=formats[i]
                                    ),
                                    named_msg,
                                )
                                self.assertTrue(
                                    m_ddp_child.weight.grad.is_contiguous(
                                        memory_format=formats[i]
                                    ),
                                    named_msg,
                                )
                                for j, ((param_name, p), p_ddp) in enumerate(
                                    zip(
                                        m_child.named_parameters(),
                                        m_ddp_child.parameters(),
                                    )
                                ):
                                    named_msg = (
                                        layer_name + "." + param_name + " " + iter_msg
                                    )
                                    self.assertEqual(
                                        p.grad, p_ddp.grad, rtol=tol, atol=tol
                                    )
                            opt.step()
                            opt_ddp.step()
                            if it == 0:
                                for p, p_ddp in zip(m.parameters(), m_ddp.parameters()):
                                    p.grad = None
                                    p_ddp.grad = None
                            else:
                                m.zero_grad()
                                m_ddp.zero_grad()
                        except BaseException:
                            # Makes sure we still get info if an error occurred somewhere other than the asserts.
                            print(
                                "Caught exception during iterations at " + named_msg,
                                flush=True,
                            )
                            raise

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @skip_if_rocm
    def test_grad_layout_1devicemodule_1replicaperprocess(self):
        dev0 = torch.device("cuda:" + str(gpus_for_rank(self.world_size)[self.rank][0]))
        # Tells DDP to use just one device.
        replica_devices = [dev0]
        # Tells _test_grad_layout to construct ConvNet with all layers on this process's first assigned device.
        layer_devs = dev0
        local_batch_size = 8
        self._test_grad_layout(replica_devices, layer_devs, local_batch_size)

    @requires_nccl()
    @skip_if_lt_x_gpu(4)
    @skip_if_rocm
    def test_grad_layout_2devicemodule(self):
        int_devices = gpus_for_rank(self.world_size)[self.rank][:2]
        dev0 = torch.device("cuda:" + str(int_devices[0]))
        dev1 = torch.device("cuda:" + str(int_devices[1]))
        # DDP's default behavior for a multi-device module is "don't replicate."
        replica_devices = None
        # Tells _test_grad_layout to constructs this process's ConvNet on 2 devices, with 2 layers on each device.
        layer_devs = [dev0] * 2 + [dev1] * 2
        local_batch_size = 8
        self._test_grad_layout(replica_devices, layer_devs, local_batch_size)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_param_layout_mismatch_error(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        dev0 = torch.device("cuda:" + str(gpus_for_rank(self.world_size)[self.rank][0]))
        layer_devs = dev0
        layer_formats = (
            [torch.contiguous_format] * 4
            if self.rank == 0
            else [torch.channels_last] * 4
        )
        layer_dtypes = [torch.float] * 4

        m = ConvNet(layer_devs, layer_formats, layer_dtypes)
        if self.rank == 0:
            m_ddp = DistributedDataParallel(
                m, device_ids=[dev0], process_group=process_group
            )
        else:
            with self.assertRaisesRegex(
                RuntimeError,
                ".* appears not to match strides of the same param in process 0",
            ):
                m_ddp = DistributedDataParallel(
                    m, device_ids=[dev0], process_group=process_group
                )

    def _gpu_model_with_ddp_comm_hook(
        self,
        process_group,
        hook=None,
        gradient_as_bucket_view=False,
        state=None,
        static_graph=False,
    ):
        device_id = gpus_for_rank(self.world_size)[self.rank][0]
        gpu_model = DistributedDataParallel(
            ModuleForDdpCommHook().to(device_id),
            device_ids=[device_id],
            process_group=process_group,
            gradient_as_bucket_view=gradient_as_bucket_view,
        )

        if static_graph:
            gpu_model._set_static_graph()

        # Register a DDP communication hook if any.
        if hook is not None:
            gpu_model.register_comm_hook(state, hook)

        return gpu_model

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_comm_hook_future_passing_gpu_nccl(self):
        """
        This unit test verifies whether the Future object is passed properly using nccl backend.
        The hook callback function creates a Future object and sets a value to it.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        # Get GPU model with simple_hook registered.
        gpu_model = self._gpu_model_with_ddp_comm_hook(process_group, self._simple_hook)

        # check whether the grads are equal to what simple_hook's then callback returns.
        # without the comm_hook, result would be 0.25 * torch.ones(2, 2).
        self._run_and_verify_hook(gpu_model, 8, 2 * torch.ones(2, 2))

    def _test_ddp_comm_hook_allreduce_hook_nccl(
        self, gradient_as_bucket_view=False, static_graph=False
    ):
        """
        This unit test verifies whether a DDP communication hook that just calls
        allreduce gives the same result with the case of no hook registered.
        Without the then callback, the future_value in reducer is no longer
        a PyObject, and this unit test verifies future_value is properly checked.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        def allreduce_hook(
            state: object, bucket: dist.GradBucket
        ) -> torch.futures.Future[torch.Tensor]:
            tensors = [bucket.buffer() / self.world_size]
            return (
                process_group.allreduce(tensors)
                .get_future()
                .then(lambda fut: fut.value()[0])
            )

        # Get GPU model with allreduce_hook registered.
        gpu_model = self._gpu_model_with_ddp_comm_hook(
            process_group, allreduce_hook, gradient_as_bucket_view, static_graph
        )

        # check whether the grads are equal to what DDP without hook would return.
        self._run_and_verify_hook(gpu_model, 8, 0.25 * torch.ones(2, 2))

    def _test_default_ddp_comm_hooks_nccl(self, gradient_as_bucket_view=False):
        """
        This unit test verifies whether default Python DDP communication hooks ALLREDUCE, FP16_COMPRESS
        and BF16_COMPRESS, can give the same result with the case of no hook registered.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        # For these default DDP comm hooks, the only state is process group.
        state = process_group
        hook_options = [default.allreduce_hook, default.fp16_compress_hook]
        if (
            not TEST_WITH_ROCM
            and BFLOAT16_AVAILABLE
            and c10d.is_nccl_available()
            and torch.cuda.nccl.version() >= (2, 10)
        ):
            hook_options.append(default.bf16_compress_hook)
        for hook in hook_options:
            # Get GPU model with the hook registered.
            # The first arg 'process_group' is used for initializing the test environment,
            # so it cannot be replaced by 'state', although they have the same value.
            gpu_model = self._gpu_model_with_ddp_comm_hook(
                process_group, hook, gradient_as_bucket_view, state
            )

            # check whether the grads are equal to what DDP without hook would return.
            self._run_and_verify_hook(gpu_model, 8, 0.25 * torch.ones(2, 2))

    def _test_fp16_compress_wrapper(self, gradient_as_bucket_view=False):
        """
        This unit test verifies whether wrapping the ALLREDUCE and POWER_SGD hooks with
        the FP16_WRAPPER can give the same result as when there is no hook registered.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        powerSGD_state = powerSGD.PowerSGDState(process_group=process_group)

        hook_args = [
            (powerSGD.powerSGD_hook, powerSGD_state),
            (default.allreduce_hook, process_group),
        ]

        for hook, state in hook_args:
            gpu_model = self._gpu_model_with_ddp_comm_hook(
                process_group,
                default.fp16_compress_wrapper(hook),
                gradient_as_bucket_view,
                state,
            )

            # check whether the grads are equal to what DDP without hook would return.
            self._run_and_verify_hook(gpu_model, 8, 0.25 * torch.ones(2, 2))

    def _test_bf16_compress_wrapper(self, gradient_as_bucket_view=False):
        """
        This unit test verifies whether wrapping the ALLREDUCE and POWER_SGD hooks with
        the BF16_WRAPPER can give the same result as when there is no hook registered.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        powerSGD_state = powerSGD.PowerSGDState(process_group=process_group)

        hook_args = [
            (powerSGD.powerSGD_hook, powerSGD_state),
            (default.allreduce_hook, process_group),
        ]

        for hook, state in hook_args:
            gpu_model = self._gpu_model_with_ddp_comm_hook(
                process_group,
                default.bf16_compress_wrapper(hook),
                gradient_as_bucket_view,
                state,
            )

            # check whether the grads are equal to what DDP without hook would return.
            self._run_and_verify_hook(gpu_model, 8, 0.25 * torch.ones(2, 2))

    def _test_hook_then_optimizer(
        self,
        functional_optim_cls,
        *functional_optim_args,
        gradient_as_bucket_view=False,
        **functional_optim_kwargs
    ):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        hook, hook_state = default.allreduce_hook, process_group
        opt_hook_state = default._OptimizerHookState(
            functional_optim_cls,
            *functional_optim_args,
            **functional_optim_kwargs,
        )
        gpu_model = self._gpu_model_with_ddp_comm_hook(
            process_group,
            default._hook_then_optimizer(hook, opt_hook_state),
            gradient_as_bucket_view,
            hook_state,
        )
        prev_params = copy.deepcopy(list(gpu_model.parameters()))
        # Run model with optimizer as part of hook
        for _ in range(8):
            gpu_model.zero_grad()
            self._run_and_verify_hook(gpu_model, 8, 0.25 * torch.ones(2, 2))
        new_params = list(gpu_model.parameters())
        # Run plain model with allreduce hook and separate optimizer step.
        # Verify gradients are the same.
        gpu_model_allreduce = self._gpu_model_with_ddp_comm_hook(
            process_group, default.allreduce_hook, gradient_as_bucket_view, hook_state
        )
        mapping = {v: k for k, v in functional_optim_map.items()}
        sgd = mapping.get(functional_optim_cls)(
            gpu_model_allreduce.parameters(),
            *functional_optim_args,
            **functional_optim_kwargs,
        )
        for _ in range(8):
            gpu_model_allreduce.zero_grad()
            self._run_and_verify_hook(gpu_model_allreduce, 8, 0.25 * torch.ones(2, 2))
            sgd.step()
        post_opt_params = list(gpu_model_allreduce.parameters())
        for opt_as_hook_param, post_opt_param in zip(new_params, post_opt_params):
            self.assertEqual(opt_as_hook_param, post_opt_param)

    def _test_powerSGD_ddp_comm_hook_nccl(self, gradient_as_bucket_view=False):
        """
        This unit test verifies whether Python DDP communication hook POWER_SGD
        can give the same result with the case of no hook registered.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        # Get GPU model with the hook registered.
        # Test the hook with different algorithmic configs.
        for use_error_feedback, warm_start in product([True, False], [True, False]):
            state = powerSGD.PowerSGDState(
                process_group=process_group,
                matrix_approximation_rank=1,
                use_error_feedback=use_error_feedback,
                warm_start=warm_start,
            )
            for hook in [powerSGD.powerSGD_hook, powerSGD.batched_powerSGD_hook]:
                gpu_model = self._gpu_model_with_ddp_comm_hook(
                    process_group, hook, gradient_as_bucket_view, state
                )

                # check whether the grads are equal to what DDP without hook would return.
                self._run_and_verify_hook(gpu_model, 8, 0.25 * torch.ones(2, 2))

    def _test_builtin_ddp_comm_hooks_nccl(self, gradient_as_bucket_view=False):
        """
        This unit test verifies whether built-in C++ DDP communication hooks ALLREDUCE and FP16_COMPRESS
        can give the same result with the case of no hook registered.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        for comm_hook_type in [
            dist.BuiltinCommHookType.ALLREDUCE,
            dist.BuiltinCommHookType.FP16_COMPRESS,
        ]:
            # Get GPU model with the built-in communication hook.
            gpu_model = self._gpu_model_with_builtin_ddp_comm_hook(
                process_group, comm_hook_type, gradient_as_bucket_view
            )

            # check whether the grads are equal to what DDP without hook would return.
            self._run_and_verify_hook(gpu_model, 8, 0.25 * torch.ones(2, 2))

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_comm_hook_allreduce_hook_nccl(self):
        self._test_ddp_comm_hook_allreduce_hook_nccl()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_default_ddp_comm_hooks_nccl(self):
        self._test_default_ddp_comm_hooks_nccl()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_fp16_compress_wrapper_nccl(self):
        self._test_fp16_compress_wrapper()

    @requires_nccl()
    @requires_nccl_version((2, 10), "Need NCCL 2.10+ for BF16_COMPRESS")
    @sandcastle_skip_if(
        not BFLOAT16_AVAILABLE,
        "BFloat16 is only supported by CUDA 11+",
    )
    @skip_if_lt_x_gpu(2)
    @skip_if_rocm
    def test_bf16_compress_wrapper_nccl(self):
        self._test_bf16_compress_wrapper()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_hook_then_sgd_nccl(self):
        sgd_lr = 1e-2
        sgd_momentum = 0.9
        sgd_weight_decay = 0.01
        self._test_hook_then_optimizer(
            _FunctionalSGD,
            sgd_lr,
            momentum=sgd_momentum,
            weight_decay=sgd_weight_decay,
        )

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_hook_then_sgd_nccl_grad_as_bucket_view(self):
        sgd_lr = 1e-2
        sgd_momentum = 0.9
        sgd_weight_decay = 0.01
        self._test_hook_then_optimizer(
            _FunctionalSGD,
            sgd_lr,
            momentum=sgd_momentum,
            weight_decay=sgd_weight_decay,
            gradient_as_bucket_view=True
        )

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_hook_then_adamw_nccl(self):
        adamw_lr = 1e-2
        adamw_betas = (0.9, 0.99)
        adamw_eps = 1e-6
        self._test_hook_then_optimizer(
            _FunctionalAdamW,
            adamw_lr,
            betas=adamw_betas,
            eps=adamw_eps,
            gradient_as_bucket_view=True
        )

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_hook_then_adam_nccl(self):
        adam_lr = 1e-2
        adam_betas = (0.9, 0.99)
        adam_eps = 1e-6
        self._test_hook_then_optimizer(
            _FunctionalAdam,
            adam_lr,
            betas=adam_betas,
            eps=adam_eps,
            gradient_as_bucket_view=True
        )

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_hook_then_adam_nccl_grad_as_bucket_view(self):
        adam_lr = 1e-2
        adam_betas = (0.9, 0.99)
        adam_eps = 1e-6
        self._test_hook_then_optimizer(
            _FunctionalAdam,
            adam_lr,
            betas=adam_betas,
            eps=adam_eps,
            gradient_as_bucket_view=True
        )

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_builtin_ddp_comm_hooks_nccl(self):
        self._test_builtin_ddp_comm_hooks_nccl()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_powerSGD_ddp_comm_hook_nccl(self):
        self._test_powerSGD_ddp_comm_hook_nccl()

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_comm_hook_allreduce_hook_nccl_grad_is_view(self):
        self._test_ddp_comm_hook_allreduce_hook_nccl(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_comm_hook_allreduce_hook_nccl_static_graph(self):
        self._test_ddp_comm_hook_allreduce_hook_nccl(static_graph=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_default_ddp_comm_hooks_nccl_is_view(self):
        self._test_default_ddp_comm_hooks_nccl(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_fp16_compress_wrapper_is_view(self):
        self._test_fp16_compress_wrapper(gradient_as_bucket_view=True)

    @requires_nccl()
    @requires_nccl_version((2, 10), "Need NCCL 2.10+ for BF16_COMPRESS")
    @sandcastle_skip_if(
        not BFLOAT16_AVAILABLE,
        "BFloat16 is only supported by CUDA 11+",
    )
    @skip_if_lt_x_gpu(2)
    @skip_if_rocm
    def test_bf16_compress_wrapper_is_view(self):
        self._test_bf16_compress_wrapper(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_builtin_ddp_comm_hooks_nccl_grad_is_view(self):
        self._test_builtin_ddp_comm_hooks_nccl(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_powerSGD_ddp_comm_hook_nccl_grad_is_view(self):
        self._test_powerSGD_ddp_comm_hook_nccl(gradient_as_bucket_view=True)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_comm_hook_allreduce_with_then_hook_nccl(self):
        """
        This unit test verifies whether a DDP communication hook that calls allreduce and then
        multiplies the result by ten and divides by two gives the expected result.
        """
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        def allreduce_with_then_hook(
            state: object, bucket: dist.GradBucket
        ) -> torch.futures.Future[torch.Tensor]:
            tensors = [bucket.buffer() / self.world_size]
            fut = process_group.allreduce(tensors).get_future()

            def mult(fut):
                # Multiply the result by 10.
                return 10 * fut.value()[0]

            def div(fut):
                # Divide the result by 2.
                return 0.5 * fut.value()

            return fut.then(mult).then(div)

        # Get GPU model with allreduce_with_then_hook registered.
        gpu_model = self._gpu_model_with_ddp_comm_hook(
            process_group, allreduce_with_then_hook
        )

        # check whether the grads are equal to what allreduce returns multuplied by 5.
        # without the comm_hook, result would be still 0.25 * torch.ones(2, 2).
        self._run_and_verify_hook(gpu_model, 8, 1.25 * torch.ones(2, 2))

    class AcceptsParam(torch.nn.Module):
        def __init__(self, p, factor):
            super().__init__()
            self.a = p
            self.f = factor

        def forward(self, input):
            return input + self.a * self.f

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_weight_sharing(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

        size = 2048 * 2048
        dev = self.rank
        world = self.world_size

        p = torch.nn.Parameter(torch.randn(size, requires_grad=True))

        for try_set_to_none, use_bucket_view in product((False, True), (False, True)):
            m = torch.nn.Sequential(
                self.AcceptsParam(p, dev + 1), self.AcceptsParam(p, dev + 1)
            ).cuda(dev)

            m = torch.nn.parallel.DistributedDataParallel(
                m,
                bucket_cap_mb=1,
                gradient_as_bucket_view=use_bucket_view,
                device_ids=[dev],
                process_group=process_group,
            )

            for i in range(3):
                m.zero_grad(set_to_none=try_set_to_none)
                m(1).sum().backward()

                # Each param value is multiplied by "rank + 1" twice in forward, so the grad
                # values produced by a particular rank should be 2. * (rank + 1).
                # Summing these over ranks and dividing by world size gives the expected result:
                analytic = torch.full_like(
                    p, 2.0 * (world * (world + 1.0) / 2.0) / world, device=dev
                )
                for name, p in m.named_parameters():
                    self.assertEqual(
                        p.grad,
                        analytic,
                        "mismatch at "
                        + name
                        + ".grad for "
                        + "set_to_none = {}, use_bucket_view = {}".format(
                            try_set_to_none, use_bucket_view
                        ),
                    )

    # A list of tests for ddp with activation checkpointing
    # when gradient_as_bucket_view=True, False.
    # Most of the tests are referred to
    # https://github.com/facebookresearch/fairscale/blob/main/tests/nn/pipe/test_checkpoint_ddp.py
    class CheckpointOnceModule(nn.Module):
        """
        Runs checkpoint for a single layer in the model.
        """
        def __init__(self):
            super().__init__()
            self.l1 = nn.Linear(20, 20)
            self.l2 = nn.Linear(20, 20)

        def forward(self, inp):
            x = self.l1(inp)
            x = checkpoint(self.l2, x)
            return x

    class CheckpointTwiceModule(CheckpointOnceModule):
        """
        Runs checkpoint for the same layer twice in a model. This simulates use
        cases such as pipeline parallel where the same layer can be checkpointed
        more than one time.
        """
        def __init__(self):
            super().__init__()

        def forward(self, inp):
            x = self.l1(inp)
            x = checkpoint(self.l2, x)
            x = checkpoint(self.l2, x)
            return x

    class CheckpointTwiceModuleWeightSharing(CheckpointTwiceModule):
        """
        Similar to CheckpointTwiceModule but the weights are shared.
        """
        def __init__(self):
            super().__init__()
            self.l1.weight = self.l2.weight

        def forward(self, inp):
            x = self.l1(inp)
            x = checkpoint(self.l2, x)
            x = checkpoint(self.l2, x)
            return x

    def _prepare_dummy_data(self):
        ddp_bs = 16
        bs = ddp_bs * self.world_size
        input = torch.rand((bs, 20), device="cuda", requires_grad=True)
        target = torch.randn((bs, 20), device="cuda")
        offset = self.rank * ddp_bs
        ddp_input = input[offset : offset + ddp_bs]
        ddp_target = target[offset : offset + ddp_bs]
        return input, ddp_input, target, ddp_target

    def _train_model(self, model, input_var, target, loss, run_checkpoint=False):
        model.train()
        if run_checkpoint:
            output = checkpoint(model, input_var)
        else:
            output = model(input_var)
        l = loss(output, target)
        l.backward()

    def _test_ddp_checkpointing(
        self,
        input_model,
        process_group,
        use_bucket_view,
        find_unused_parameters=False,
        static_graph=False,
        run_checkpoint=False,
    ):
        # to reproduce the same training results
        torch.cuda.set_device(self.rank)
        torch.manual_seed(31415)
        model = copy.deepcopy(input_model).cuda()
        ddp_model = copy.deepcopy(input_model).cuda()
        ddp_model = nn.parallel.DistributedDataParallel(
            ddp_model,
            bucket_cap_mb=1,
            gradient_as_bucket_view=use_bucket_view,
            device_ids=[self.rank],
            process_group=process_group,
            find_unused_parameters=find_unused_parameters,
        )
        if static_graph:
            ddp_model._set_static_graph()
        self.assertEqual(
            ddp_model._get_ddp_logging_data().get("static_graph", 0), static_graph
        )
        input, ddp_input, target, ddp_target = self._prepare_dummy_data()
        loss = nn.MSELoss()
        for i in range(5):
            model.zero_grad(set_to_none=False)
            ddp_model.zero_grad(set_to_none=False)
            self._train_model(model, input, target, loss, run_checkpoint=run_checkpoint)
            self._train_model(
                ddp_model, ddp_input, ddp_target, loss, run_checkpoint=run_checkpoint
            )
            for i, j in zip(model.parameters(), ddp_model.parameters()):
                self.assertTrue(i.grad is not None)
                self.assertTrue(j.grad is not None)
                self.assertEqual(i.grad, j.grad, rtol=1.3e-06, atol=5e-5)

    # DDP works as expect when layer is checkpointed only once,
    # when find_unused_parameters=False.
    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_checkpointing_once(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        for use_bucket_view, static_graph in product((False, True), (False, True)):
            self._test_ddp_checkpointing(
                self.CheckpointOnceModule(),
                process_group=process_group,
                use_bucket_view=use_bucket_view,
                static_graph=static_graph,
            )
            if static_graph:
                # find_unused_parameters does not make a difference, since it is
                # ignored for static graph.
                self._test_ddp_checkpointing(
                    self.CheckpointOnceModule(),
                    process_group=process_group,
                    use_bucket_view=use_bucket_view,
                    static_graph=static_graph,
                    find_unused_parameters=True,
                )

    # DDP will fail when there are unused_parameters in the model and we are not
    # using static graph training.
    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_checkpointing_unused_params(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        for use_bucket_view in (True, False):
            with self.assertRaisesRegex(
                RuntimeError,
                "Expected to mark a variable ready only once.",
            ):
                model = self._test_ddp_checkpointing(
                    self.CheckpointOnceModule(),
                    process_group=process_group,
                    use_bucket_view=use_bucket_view,
                    find_unused_parameters=True,
                )

    # DDP will fail when the same layer is checkpointed twice, for both settings
    # of find_unused_parameters, and non-static graph.
    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_checkpointing_twice_non_static_graph(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        for use_bucket_view in (True, False):
            error_ctx = self.assertRaisesRegex(
                RuntimeError,
                "Expected to mark a variable ready only once.",
            )

            with error_ctx:
                model = self._test_ddp_checkpointing(
                    self.CheckpointTwiceModule(),
                    process_group=process_group,
                    use_bucket_view=use_bucket_view,
                    static_graph=False,
                )

            with error_ctx:
                model = self._test_ddp_checkpointing(
                    self.CheckpointTwiceModule(),
                    process_group=process_group,
                    use_bucket_view=use_bucket_view,
                    static_graph=False,
                    find_unused_parameters=True,
                )

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_checkpointing_twice_static_graph(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        for use_bucket_view in (True, False):
            # Test passes when static_graph=True.
            model = self._test_ddp_checkpointing(
                self.CheckpointTwiceModule(),
                process_group=process_group,
                use_bucket_view=use_bucket_view,
                static_graph=True,
            )

    # DDP works as expected if there is weight sharing among layers and we
    # checkpoint once.
    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_checkpointing_weight_sharing(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        torch.cuda.set_device(self.rank)
        for use_bucket_view, static_graph in product((False, True), (False, True)):
            torch.manual_seed(31415)
            l1 = nn.Linear(20, 20)
            l2 = nn.Linear(20, 20)
            l1.weight = l2.weight
            model = nn.Sequential(l1, l2)
            self._test_ddp_checkpointing(
                model,
                process_group=process_group,
                use_bucket_view=use_bucket_view,
                static_graph=static_graph,
                run_checkpoint=True,
            )

    # Checkpointing should work with static graph
    # in the case of checkpointing same layer twice and
    # having weights shared across layers.
    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_ddp_checkpoint_twice_weight_sharing(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        torch.cuda.set_device(self.rank)
        for use_bucket_view in (True, False):
            model = self._test_ddp_checkpointing(
                self.CheckpointTwiceModuleWeightSharing(),
                process_group=process_group,
                use_bucket_view=use_bucket_view,
                static_graph=True,
            )



class NcclErrorHandlingTest(MultiProcessTestCase):
    def setUp(self):
        super(NcclErrorHandlingTest, self).setUp()
        # Need to skip return code checking for these tests since the child
        # processes don't exit cleanly.
        self.skip_return_code_checks = [
            self.test_nccl_errors_blocking_abort.__wrapped__,
            self.test_nccl_errors_blocking_sigkill.__wrapped__,
            self.test_nccl_errors_blocking_sigterm.__wrapped__,
            self.test_nccl_errors_blocking_nonzero_exit.__wrapped__,
        ]
        # NCCL_BLOCKING_WAIT overrides NCCL_ASYNC_ERROR_HANDLING hence tests
        # that use NCCL_BLOCKING_WAIT will test it as expected.
        os.environ["NCCL_ASYNC_ERROR_HANDLING"] = "1"
        self._spawn_processes()

    def tearDown(self):
        super(NcclErrorHandlingTest, self).tearDown()
        try:
            os.remove(self.file_name)
        except OSError:
            pass

    @property
    def op_timeout_sec(self):
        return 1

    @property
    def world_size(self):
        return 3

    @property
    def blocking_wait_error_msg(self):
        return "Caught collective operation timeout"

    def _run_all_reduce(self, pg):
        pg.allreduce(torch.rand(10).cuda(self.rank))

    @requires_nccl()
    @requires_nccl_version((2, 4, 0), "Need NCCL 2.4+ for error checking")
    @skip_if_lt_x_gpu(3)
    @skip_if_rocm
    @sandcastle_skip("Test does not pass when run locally")
    def test_nccl_errors_nonblocking(self):
        # Note: we unset and restore NCCL_ASYNC_ERROR_HANDLING for this test
        # since test_c10d_common runs with async error handling by default, but this
        # tests behavior when it is not enabled.
        prev_nccl_async_error_handling = os.environ.get(
            "NCCL_ASYNC_ERROR_HANDLING", None
        )
        os.environ["NCCL_ASYNC_ERROR_HANDLING"] = "0"
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        process_group.allreduce(torch.rand(10).cuda(self.rank))
        if self.rank == 0:
            # This allreduce does not block Python thread as allreduce enqueues
            # the cuda operation, and then wait only blocks the current cuda
            # stream.
            work = process_group.allreduce(torch.rand(10).cuda(self.rank))
            work.wait()

            # Now the work scheduled next should hang forever since the previous
            # allreduce will never complete.
            t = threading.Thread(target=self._run_all_reduce, args=(process_group,))
            t.daemon = True
            t.start()
            t.join(int(get_timeout(self.id()) / 5))
            self.assertTrue(t.is_alive())

        if prev_nccl_async_error_handling is not None:
            os.environ["NCCL_ASYNC_ERROR_HANDLING"] = prev_nccl_async_error_handling

    def _test_nccl_errors_blocking(self, func):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(
            store,
            self.rank,
            self.world_size,
            timeout=timedelta(seconds=10),
        )
        process_group.allreduce(torch.rand(10).cuda(self.rank))
        if self.rank == 0:
            work = process_group.allreduce(torch.rand(10).cuda(self.rank))
            with self.assertRaisesRegex(RuntimeError, self.blocking_wait_error_msg):
                # Operation would time out in blocking mode.
                work.wait(timeout=timedelta(seconds=self.op_timeout_sec))
            # Run some GPU operations to make sure cuda has not gotten stuck.
            # It was observed cuda could get stuck if NCCL communicators were
            # not properly aborted before throwing RuntimeError.
            a = torch.rand(10).cuda(self.rank)
        elif self.rank == 1:
            # Clean up structures (ex: files for FileStore before going down)
            del process_group
            func()
        else:
            # Wait for timeout
            time.sleep(2 * self.op_timeout_sec)

            # Now verify communicators on this rank have been aborted by the watchdog thread.
            self._wait_for_comm_abort(process_group)

    @with_nccl_blocking_wait
    @requires_nccl()
    @requires_nccl_version((2, 4, 0), "Need NCCL 2.4+ for error checking")
    @skip_if_lt_x_gpu(3)
    @skip_if_rocm
    def test_nccl_errors_blocking_clean_exit(self):
        self._test_nccl_errors_blocking(lambda: sys.exit(0))

    @with_nccl_blocking_wait
    @requires_nccl()
    @requires_nccl_version((2, 4, 0), "Need NCCL 2.4+ for error checking")
    @skip_if_lt_x_gpu(3)
    @skip_if_rocm
    def test_nccl_errors_blocking_nonzero_exit(self):
        self._test_nccl_errors_blocking(lambda: sys.exit(1))

    @with_nccl_blocking_wait
    @requires_nccl()
    @requires_nccl_version((2, 4, 0), "Need NCCL 2.4+ for error checking")
    @skip_if_lt_x_gpu(3)
    @skip_if_rocm
    @sandcastle_skip(
        "Frequently times out see https://github.com/pytorch/pytorch/issues/58920"
    )
    def test_nccl_errors_blocking_abort(self):
        self._test_nccl_errors_blocking(lambda: os.abort())

    @with_nccl_blocking_wait
    @requires_nccl()
    @requires_nccl_version((2, 4, 0), "Need NCCL 2.4+ for error checking")
    @skip_if_lt_x_gpu(3)
    @skip_if_rocm
    def test_nccl_errors_blocking_sigkill(self):
        self._test_nccl_errors_blocking(lambda: os.kill(os.getpid(), signal.SIGKILL))

    @with_nccl_blocking_wait
    @requires_nccl()
    @requires_nccl_version((2, 4, 0), "Need NCCL 2.4+ for error checking")
    @skip_if_lt_x_gpu(3)
    @skip_if_rocm
    def test_nccl_errors_blocking_sigterm(self):
        self._test_nccl_errors_blocking(lambda: os.kill(os.getpid(), signal.SIGTERM))

    @with_nccl_blocking_wait
    @requires_nccl()
    @requires_nccl_version((2, 4, 0), "Need NCCL 2.4+ for error checking")
    @skip_if_lt_x_gpu(3)
    def test_nccl_blocking_wait_with_barrier(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(
            store,
            self.rank,
            self.world_size,
            timeout=timedelta(seconds=10),
        )
        process_group.barrier().wait()
        if self.rank == 0:
            with self.assertRaisesRegex(RuntimeError, self.blocking_wait_error_msg):
                # This should timeout
                process_group.barrier().wait(timeout=timedelta(seconds=self.op_timeout_sec))

    def _run_invalid_nccl_blocking_wait_env(self, val):
        os.environ["NCCL_BLOCKING_WAIT"] = val
        store = c10d.FileStore(self.file_name, self.world_size)
        with self.assertRaises(RuntimeError):
            process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)

    @requires_nccl()
    @skip_if_lt_x_gpu(3)
    def test_invalid_nccl_blocking_wait_env(self):
        self._run_invalid_nccl_blocking_wait_env("abc")
        self._run_invalid_nccl_blocking_wait_env("-1")
        self._run_invalid_nccl_blocking_wait_env("2147483647")
        self._run_invalid_nccl_blocking_wait_env("4294967295")

    def _check_valid_comm_exception(self, e):
        exception_str = str(e)
        valid_exceptions = [
            "NCCL communicator was aborted",
            "NCCL communicator encountered error",
            "Caught collective operation timeout"
        ]
        return any(exc in exception_str for exc in valid_exceptions)

    def _wait_for_comm_abort(self, process_group, timeout=None):
        """
        Waits for the watchdog thread to abort communicators for the process group.
        """
        while True:
            try:
                if not timeout:
                    process_group.allreduce(torch.rand(10).cuda(self.rank)).wait()
                else:
                    assert isinstance(timeout, timedelta)
                    process_group.allreduce(torch.rand(10).cuda(self.rank)).wait(timeout=timeout)
            except Exception as e:
                if self._check_valid_comm_exception(e):
                    return
                else:
                    raise e
            time.sleep(1)

    @with_nccl_blocking_wait
    @requires_nccl()
    @requires_gloo()
    @skip_if_lt_x_gpu(3)
    def test_nccl_timeout(self):
        store = c10d.FileStore(self.file_name, self.world_size)

        # Initialize process_group.
        process_group = c10d.ProcessGroupNCCL(
            store, self.rank, self.world_size, timeout=timedelta(seconds=10)
        )
        # Control gloo pg used as go-ahead signal/barrier
        # to coordinate btwn ranks.
        pg_gloo = c10d.ProcessGroupGloo(store, self.rank, self.world_size)
        failed_collective_timeout = timedelta(milliseconds=100)
        process_group.allreduce(torch.rand(10).cuda(self.rank)).wait(timeout=timedelta(seconds=5))

        if self.rank == 0:
            # This should timeout in about 1 second.
            # Watchdog may abort timed out work resulting in NCCL error instead of operation timed out.
            with self.assertRaisesRegex(RuntimeError, self.blocking_wait_error_msg):
                process_group.allreduce(torch.rand(10).cuda(self.rank)).wait(timeout=failed_collective_timeout)
            # Now do a barrier to tell other rank to go ahead.
            pg_gloo.barrier().wait()
        else:
            # Wait on rank 0 to fail.
            try:
                pg_gloo.barrier().wait()
            except Exception as e:
                raise ValueError(f"Rank {self.rank} barrier timed out waiting for rank 0 with error: {str(e)}")
            # Now verify communicators on this rank have
            # been aborted by watchdog.
            self._wait_for_comm_abort(process_group, failed_collective_timeout)


class CommTest(test_c10d_common.AbstractCommTest, MultiProcessTestCase):
    def setUp(self):
        super(CommTest, self).setUp()
        # NCCL_BLOCKING_WAIT overrides NCCL_ASYNC_ERROR_HANDLING hence tests
        # that use NCCL_BLOCKING_WAIT will test it as expected.
        os.environ["NCCL_ASYNC_ERROR_HANDLING"] = "1"
        self._spawn_processes()

    def tearDown(self):
        super(CommTest, self).tearDown()
        try:
            os.remove(self.file_name)
        except OSError:
            pass

    def _test_broadcast_coalesced(self, process_group, device, root_rank):
        half = torch.float16

        # No support for float16 for CPU tensors
        if device == torch.device("cpu"):
            half = torch.float32

        target = torch.arange(60, dtype=half, device=device).chunk(5)
        target += torch.arange(60, dtype=torch.float32, device=device).chunk(5)
        target += torch.arange(60, dtype=half, device=device).chunk(5)
        target += torch.arange(60, dtype=torch.float64, device=device).chunk(5)
        target += torch.arange(60, dtype=half, device=device).chunk(5)
        target += torch.arange(60, dtype=torch.float32, device=device).chunk(5)

        # The tensors to pass to broadcast are idential to the target
        # only on the process that is the root of the broadcast.
        if self.rank == root_rank:
            tensors = list(tensor.clone() for tensor in target)
        else:
            tensors = list(torch.zeros_like(tensor) for tensor in target)

        if self.rank != root_rank:
            self.assertNotEqual(tensors, target)

        c10d._broadcast_coalesced(
            process_group, tensors, buffer_size=256, src=root_rank
        )

        if self.rank != root_rank:
            self.assertEqual(tensors, target)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_broadcast_coalesced_nccl(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        process_group = c10d.ProcessGroupNCCL(store, self.rank, self.world_size)
        device = torch.device("cuda:%d" % self.rank)
        ranks = [0, 1]
        for root_rank in ranks:
            self._test_broadcast_coalesced(process_group, device, root_rank)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_sequence_num_set_default_pg_nccl(self):
        torch.cuda.set_device(self.rank)
        self._test_sequence_num_set_default_pg(backend="nccl")

    @skip_if_lt_x_gpu(2)
    @requires_nccl()
    def test_sequence_num_incremented_nccl_default(self):
        self._test_sequence_num_incremented_default_group("nccl")

    @skip_if_lt_x_gpu(4)
    @requires_nccl()
    def test_sequence_num_incremented_nccl_subgroup(self):
        if self.world_size < 4:
            return sandcastle_skip("Test requires world_size of at least 4")
        self._test_sequence_num_incremented_subgroup("nccl")

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_sequence_num_set_nccl_new_group(self):
        torch.cuda.set_device(self.rank)
        self._test_sequence_num_set_new_group(backend="nccl")

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_pass_nccl_options_high_priority_stream(self):
        pg_opts = c10d.ProcessGroupNCCL.Options()
        pg_opts.is_high_priority_stream = True

        store = c10d.FileStore(self.file_name, self.world_size)
        # Test init_process_group accepts options
        dist.init_process_group(
            "nccl",
            world_size=self.world_size,
            rank=self.rank,
            store=store,
            pg_options=pg_opts,
        )

        # Test with new_group
        pg = c10d.new_group([0, 1], pg_options=pg_opts)
        # test if the process group constructed with high priority stream
        self.assertTrue(pg.options.is_high_priority_stream)
        # test the process group works as expected
        t = torch.tensor([self.rank + 1] * 10).cuda(self.rank)
        pg.allreduce(t).wait()
        expected_tensor = torch.tensor([3] * 10).cuda(self.rank)
        self.assertEqual(expected_tensor, t)

    @requires_nccl()
    @skip_if_lt_x_gpu(4)
    def test_nccl_barrier(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        c10d.init_process_group(
            backend="nccl", rank=self.rank, world_size=self.world_size, store=store
        )

        t = torch.tensor([self.rank + 1] * 10).cuda(2 * self.rank)
        c10d.all_reduce(t)
        expected_tensor = torch.tensor([3] * 10).cuda(2 * self.rank)
        self.assertEqual(expected_tensor, t)

        # Test with new_group
        pg = c10d.new_group([0, 1])
        t = torch.tensor([self.rank + 1] * 10).cuda(2 * self.rank)
        pg.allreduce(t).wait()
        self.assertEqual(expected_tensor, t)

        pg = c10d.new_group([0])
        if self.rank == 0:
            t = torch.tensor([self.rank + 1] * 10).cuda(2 * self.rank)
            expected_tensor = torch.tensor([self.rank + 1] * 10).cuda(2 * self.rank)
            pg.allreduce(t).wait()
            self.assertEqual(expected_tensor, t)

        pg = c10d.new_group([1])
        if self.rank == 1:
            t = torch.tensor([self.rank + 1] * 10).cuda(2 * self.rank)
            expected_tensor = torch.tensor([self.rank + 1] * 10).cuda(2 * self.rank)
            pg.allreduce(t).wait()
            self.assertEqual(expected_tensor, t)

    @requires_nccl()
    @skip_if_lt_x_gpu(4)
    def test_nccl_barrier_timeout(self):
        os.environ["ENABLE_NCCL_HEALTH_CHECK"] = "1"
        store = c10d.FileStore(self.file_name, self.world_size)
        if self.rank == 0:
            with self.assertRaisesRegex(
                RuntimeError, "Health check failure"
            ):
                c10d.init_process_group(
                    backend="nccl",
                    rank=self.rank,
                    world_size=self.world_size,
                    store=store,
                    timeout=timedelta(seconds=10),
                )

    @requires_nccl()
    @skip_if_lt_x_gpu(4)
    def test_nccl_barrier_timeout_new_group(self):
        os.environ["ENABLE_NCCL_HEALTH_CHECK"] = "1"
        store = c10d.FileStore(self.file_name, self.world_size)
        c10d.init_process_group(
            backend="nccl",
            rank=self.rank,
            world_size=self.world_size,
            store=store,
            timeout=timedelta(seconds=10),
        )

        if self.rank == 0:
            with self.assertRaisesRegex(
                RuntimeError, "Health check failure"
            ):
                c10d.new_group([0, 1], timeout=timedelta(seconds=1))

            with self.assertRaisesRegex(
                RuntimeError, "Timed out initializing process group"
            ):
                c10d.new_group([0], timeout=timedelta(seconds=1))

    @requires_nccl()
    @skip_if_lt_x_gpu(4)
    def test_nccl_barrier_timeout_new_group_non_member(self):
        os.environ["ENABLE_NCCL_HEALTH_CHECK"] = "1"
        store = c10d.FileStore(self.file_name, self.world_size)
        c10d.init_process_group(
            backend="nccl",
            rank=self.rank,
            world_size=self.world_size,
            store=store,
            timeout=timedelta(seconds=10),
        )

        if self.rank == 1:
            with self.assertRaisesRegex(
                RuntimeError, "Health check failure"
            ):
                c10d.new_group([0, 1], timeout=timedelta(seconds=1))

            with self.assertRaisesRegex(
                RuntimeError, "Timed out initializing process group"
            ):
                c10d.new_group([0], timeout=timedelta(seconds=1))

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_nccl_barrier_device_ids(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        c10d.init_process_group(
            backend="nccl", rank=self.rank, world_size=self.world_size, store=store
        )

        c10d.barrier(device_ids=[self.rank])

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    def test_nccl_barrier_device_ids_function_argument(self):
        store = c10d.FileStore(self.file_name, self.world_size)
        c10d.init_process_group(
            backend="nccl", rank=self.rank, world_size=self.world_size, store=store
        )

        with self.assertRaisesRegex(RuntimeError, "Invalid function argument"):
            c10d.barrier(device_ids=self.rank)

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @with_dist_debug_levels(levels=["DETAIL"])
    def test_nccl_warn_not_in_group_debug_detail(self):
        self._test_warn_not_in_group(backend="nccl")

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @with_dist_debug_levels(levels=["INFO"])
    def test_nccl_warn_not_in_group_debug_info(self):
        self._test_warn_not_in_group(backend="nccl")

    @requires_nccl()
    @skip_if_lt_x_gpu(2)
    @with_dist_debug_levels(levels=["OFF"])
    def test_nccl_warn_not_in_group_debug_off(self):
        self._test_warn_not_in_group(backend="nccl")

if __name__ == "__main__":
    assert (
        not torch.cuda._initialized
    ), "test_distributed must not have initialized CUDA context on main process"

    run_tests()
