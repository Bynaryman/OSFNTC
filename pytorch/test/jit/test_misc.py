# Owner(s): ["oncall: jit"]

from typing import Any, Dict, List, Optional, Tuple

from torch.testing._internal.jit_utils import JitTestCase, make_global
from torch.testing import FileCheck
from torch import jit
from jit.test_module_interface import TestModuleInterface  # noqa: F401
import unittest
import os
import sys
import torch
import torch.testing._internal.jit_utils
import torch.nn as nn

# Make the helper files in test/ importable
pytorch_test_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
sys.path.append(pytorch_test_dir)

if __name__ == '__main__':
    raise RuntimeError("This test file is not meant to be run directly, use:\n\n"
                       "\tpython test/test_jit.py TESTNAME\n\n"
                       "instead.")

class TestMisc(JitTestCase):
    def test_joined_str(self):
        def func(x):
            hello, test = "Hello", "test"
            print(f"{hello + ' ' + test}, I'm a {test}")
            print("format blank")
            hi = 'hi'
            print(f"stuff before {hi}")
            print(f"{hi} stuff after")
            return x + 1

        x = torch.arange(4., requires_grad=True)
        # TODO: Add support for f-strings in string parser frontend
        # self.checkScript(func, [x], optimize=True, capture_output=True)

        with self.capture_stdout() as captured:
            out = func(x)

        scripted = torch.jit.script(func)
        with self.capture_stdout() as captured_script:
            out_script = func(x)

        self.assertEqual(out, out_script)
        self.assertEqual(captured, captured_script)

    @unittest.skipIf(sys.version_info[:2] < (3, 7), "`dataclasses` module not present on < 3.7")
    def test_dataclass_error(self):
        from dataclasses import dataclass

        @dataclass
        class NormalizationInfo(object):
            mean: float = 0.0

            def compute(self, total_rows):
                return self.mean

        def fn():
            return NormalizationInfo(1, 2, 3, 4, 5)

        with self.assertRaisesRegex(OSError, "could not get source code"):
            torch.jit.script(fn)


    def test_kwarg_support(self):
        with self.assertRaisesRegex(torch.jit.frontend.NotSupportedError, "variable number of arguments"):
            class M(torch.nn.Module):
                def forward(self, *, n_tokens: int, device_name: str = 2):
                    pass
            torch.jit.script(M())

        class M(torch.nn.Module):
            def forward(self, *, n_tokens: int, device_name: str):
                return n_tokens, device_name

        sm = torch.jit.script(M())

        with self.assertRaisesRegex(RuntimeError, "missing value for argument 'n_tokens'"):
            sm()

        with self.assertRaisesRegex(RuntimeError, "positional arg"):
            sm(3, 'hello')

        self.assertEqual(sm(n_tokens=3, device_name='hello'), (3, 'hello'))

    def test_tuple_subscripted_assign(self):
        with self.assertRaisesRegex(RuntimeError, "subscripted assignment"):
            @torch.jit.script
            def foo(a: Tuple[int, int]) -> None:
                a[0] = a[1]

        with self.assertRaisesRegex(RuntimeError, "augmented assignment"):
            @torch.jit.script
            def bar(a: Tuple[int, int]) -> None:
                a[0] += a[1]

    def test_subexpression_List_Future(self):

        @torch.jit.script
        def fn(x: List[torch.jit.Future[int]]) -> torch.jit.Future[int]:
            return x[0]

        FileCheck().check('Future[int]').check('Future[int]').run(fn.graph)

    def test_subexpression_Future_annotate(self):
        @torch.jit.script
        def fn() -> torch.jit.Future[int]:
            x: List[torch.jit.Future[int]] = []
            return x[0]

        FileCheck().check("Future[int][]").run(fn.graph)

    def test_future_isinstance(self):
        @torch.jit.script
        def fn(x: Any) -> torch.jit.Future[int]:
            assert isinstance(x, jit.Future[int])
            return x

        FileCheck().check("Future[int]").run(fn.graph)

    def test_str_refine_any(self):
        def forward(x: Any) -> str:
            if isinstance(x, str):
                return x
            return "foo"
        forward = torch.jit.script(forward)
        self.assertEqual(forward(1), "foo")
        self.assertEqual(forward("bar"), "bar")

    def test_subexpression_Tuple_int_int_Future(self):

        @torch.jit.script
        def fn(x: Tuple[int, int, torch.jit.Future[int]]) -> Tuple[int, torch.jit.Future[int]]:
            return x[0], x[2]

        FileCheck().check('(int, int, Future[int])').check('(int, Future[int])').run(fn.graph)

    def test_subexpression_Dict_int_Future(self):

        @torch.jit.script
        def fn(x: Dict[int, torch.jit.Future[int]], y: int) -> torch.jit.Future[int]:
            return x[y]

        FileCheck().check('Dict(int, Future(int))').check('Future[int]').run(fn.graph)

    def test_subexpression_Optional(self):

        @torch.jit.script
        def fn(x: Optional[Dict[int, torch.jit.Future[int]]]) -> Optional[torch.jit.Future[int]]:
            if x is not None:
                return x[0]
            else:
                return None

        FileCheck().check('Dict(int, Future(int))?').run(fn.graph)

    def test_if_returning_any(self):
        """
        Check that an if statement can return different
        types early from each branch when the return
        type of the function is Any.
        """
        def if_function(inp: torch.Tensor) -> Any:
            if inp.shape[0] == 1:
                return inp * inp
            else:
                return "str"

        self.checkScript(if_function, (torch.randn(5),))

    def test_export_opnames_interface(self):

        @torch.jit.interface
        class OneTwoModule(nn.Module):
            def one(self, x: torch.Tensor, y: torch.Tensor) -> torch.Tensor:
                pass

            def two(self, x: torch.Tensor) -> torch.Tensor:
                pass

            def forward(self, x: torch.Tensor) -> torch.Tensor:
                pass

        class FooMod(nn.Module):
            def one(self, x: torch.Tensor, y: torch.Tensor) -> torch.Tensor:
                return x + y

            def two(self, x: torch.Tensor) -> torch.Tensor:
                return 2 * x

            def forward(self, x: torch.Tensor) -> torch.Tensor:
                return self.one(self.two(x), x)

        class BarMod(nn.Module):
            def one(self, x: torch.Tensor, y: torch.Tensor) -> torch.Tensor:
                return x * y

            def two(self, x: torch.Tensor) -> torch.Tensor:
                return 2 / x

            def forward(self, x: torch.Tensor) -> torch.Tensor:
                return self.two(self.one(x, x))

        make_global(OneTwoModule)

        class M(nn.Module):
            sub : OneTwoModule

            def __init__(self):
                super(M, self).__init__()
                self.sub = BarMod()

            def forward(self, x: torch.Tensor) -> torch.Tensor:
                return self.sub.forward(x)

        def use_module_interface(mod_list: List[OneTwoModule], x: torch.Tensor):
            return mod_list[0].forward(x) + mod_list[1].forward(x)

        torch._C._enable_mobile_interface_call_export()
        scripted_M_mod = torch.jit.script(M())
        self.assertTrue(set(['aten::mul.Scalar', 'aten::mul.Tensor', 'aten::reciprocal']).issubset(
            set(torch.jit.export_opnames(scripted_M_mod))))

        scripted_M_mod.sub = torch.jit.script(FooMod())
        self.assertTrue(set(['aten::add.Tensor', 'aten::mul.Scalar']).issubset(
            set(torch.jit.export_opnames(scripted_M_mod))))

    def test_broadcasting_list(self):
        """
        Test BroadcastingList and torch.nn._size_N_t alias
        """
        from torch._jit_internal import BroadcastingList2
        from torch.nn.common_types import _size_2_t

        def sum_i(x: _size_2_t) -> int:
            return x[0] + x[1]

        def sum_f(x: BroadcastingList2[float]) -> float:
            return x[0] + x[1]

        self.assertTrue(torch.jit.script(sum_i)(4) == 8)
        self.assertTrue(torch.jit.script(sum_f)(4.5) == 9.)
