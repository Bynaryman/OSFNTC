--------------------------------------------------------------------------------
--                                Arith_to_S3
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: BSC / UPC - Ledoux Louis
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: arith_i
-- Output signals: S3_o

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity Arith_to_S3 is
    port (clk : in std_logic;
          arith_i : in  std_logic_vector(15 downto 0);
          S3_o : out  std_logic_vector(17 downto 0)   );
end entity;

architecture arch of Arith_to_S3 is
signal sign :  std_logic;
signal exponent :  std_logic_vector(7 downto 0);
signal fraction :  std_logic_vector(6 downto 0);
signal isNaN :  std_logic;
signal isExpSubnormalZero :  std_logic;
signal implicit :  std_logic;
signal final_scale :  std_logic_vector(7 downto 0);
begin
   sign <= arith_i(15);
   exponent <= arith_i(14 downto 7);
   fraction <= arith_i(6 downto 0);
   isNaN <= '1' when exponent="11111111" else '0';
   isExpSubnormalZero <= '1' when exponent="00000000" else '0';
   implicit <= not(isExpSubnormalZero);
   final_scale<= "00000001" when isExpSubnormalZero= '1' else  exponent;
   S3_o <= isNaN & sign & implicit & fraction & final_scale;
end architecture;

--------------------------------------------------------------------------------
--            LZOCShifterSticky_528_to_9_counting_1024_F250_uid20
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Florent de Dinechin, Bogdan Pasca (2007-2016)
--------------------------------------------------------------------------------
-- Pipeline depth: 7 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: I OZb
-- Output signals: Count O Sticky

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity LZOCShifterSticky_528_to_9_counting_1024_F250_uid20 is
    port (clk : in std_logic;
          I : in  std_logic_vector(527 downto 0);
          OZb : in  std_logic;
          Count : out  std_logic_vector(9 downto 0);
          O : out  std_logic_vector(8 downto 0);
          Sticky : out  std_logic   );
end entity;

architecture arch of LZOCShifterSticky_528_to_9_counting_1024_F250_uid20 is
signal level10, level10_d1, level10_d2 :  std_logic_vector(527 downto 0);
signal sozb, sozb_d1, sozb_d2, sozb_d3, sozb_d4, sozb_d5, sozb_d6, sozb_d7 :  std_logic;
signal sticky10, sticky10_d1, sticky10_d2 :  std_logic;
signal count9, count9_d1, count9_d2, count9_d3, count9_d4, count9_d5 :  std_logic;
signal level9, level9_d1, level9_d2 :  std_logic_vector(527 downto 0);
signal sticky_high_9, sticky_high_9_d1, sticky_high_9_d2 :  std_logic;
signal sticky_low_9, sticky_low_9_d1, sticky_low_9_d2 :  std_logic;
signal sticky9, sticky9_d1, sticky9_d2 :  std_logic;
signal count8, count8_d1, count8_d2, count8_d3 :  std_logic;
signal level8, level8_d1 :  std_logic_vector(510 downto 0);
signal sticky_high_8, sticky_high_8_d1, sticky_high_8_d2 :  std_logic;
signal sticky_low_8, sticky_low_8_d1, sticky_low_8_d2, sticky_low_8_d3, sticky_low_8_d4 :  std_logic;
signal sticky8, sticky8_d1 :  std_logic;
signal count7, count7_d1, count7_d2 :  std_logic;
signal level7, level7_d1 :  std_logic_vector(254 downto 0);
signal sticky_high_7, sticky_high_7_d1 :  std_logic;
signal sticky_low_7, sticky_low_7_d1 :  std_logic;
signal sticky7, sticky7_d1 :  std_logic;
signal count6, count6_d1 :  std_logic;
signal level6 :  std_logic_vector(126 downto 0);
signal sticky_high_6, sticky_high_6_d1 :  std_logic;
signal sticky_low_6, sticky_low_6_d1 :  std_logic;
signal sticky6 :  std_logic;
signal count5, count5_d1 :  std_logic;
signal level5, level5_d1 :  std_logic_vector(62 downto 0);
signal sticky_high_5 :  std_logic;
signal sticky_low_5 :  std_logic;
signal sticky5, sticky5_d1 :  std_logic;
signal count4 :  std_logic;
signal level4 :  std_logic_vector(30 downto 0);
signal sticky_high_4, sticky_high_4_d1 :  std_logic;
signal sticky_low_4, sticky_low_4_d1 :  std_logic;
signal sticky4 :  std_logic;
signal count3 :  std_logic;
signal level3 :  std_logic_vector(15 downto 0);
signal sticky_high_3 :  std_logic;
signal sticky_low_3 :  std_logic;
signal sticky3 :  std_logic;
signal count2 :  std_logic;
signal level2 :  std_logic_vector(11 downto 0);
signal sticky_high_2 :  std_logic;
signal sticky_low_2, sticky_low_2_d1, sticky_low_2_d2, sticky_low_2_d3, sticky_low_2_d4, sticky_low_2_d5, sticky_low_2_d6, sticky_low_2_d7 :  std_logic;
signal sticky2 :  std_logic;
signal count1 :  std_logic;
signal level1 :  std_logic_vector(9 downto 0);
signal sticky_high_1 :  std_logic;
signal sticky_low_1, sticky_low_1_d1, sticky_low_1_d2, sticky_low_1_d3, sticky_low_1_d4, sticky_low_1_d5, sticky_low_1_d6, sticky_low_1_d7 :  std_logic;
signal sticky1 :  std_logic;
signal count0 :  std_logic;
signal level0 :  std_logic_vector(8 downto 0);
signal sticky_high_0 :  std_logic;
signal sticky_low_0, sticky_low_0_d1, sticky_low_0_d2, sticky_low_0_d3, sticky_low_0_d4, sticky_low_0_d5, sticky_low_0_d6, sticky_low_0_d7 :  std_logic;
signal sticky0 :  std_logic;
signal sCount :  std_logic_vector(9 downto 0);
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            level10_d1 <=  level10;
            level10_d2 <=  level10_d1;
            sozb_d1 <=  sozb;
            sozb_d2 <=  sozb_d1;
            sozb_d3 <=  sozb_d2;
            sozb_d4 <=  sozb_d3;
            sozb_d5 <=  sozb_d4;
            sozb_d6 <=  sozb_d5;
            sozb_d7 <=  sozb_d6;
            sticky10_d1 <=  sticky10;
            sticky10_d2 <=  sticky10_d1;
            count9_d1 <=  count9;
            count9_d2 <=  count9_d1;
            count9_d3 <=  count9_d2;
            count9_d4 <=  count9_d3;
            count9_d5 <=  count9_d4;
            level9_d1 <=  level9;
            level9_d2 <=  level9_d1;
            sticky_high_9_d1 <=  sticky_high_9;
            sticky_high_9_d2 <=  sticky_high_9_d1;
            sticky_low_9_d1 <=  sticky_low_9;
            sticky_low_9_d2 <=  sticky_low_9_d1;
            sticky9_d1 <=  sticky9;
            sticky9_d2 <=  sticky9_d1;
            count8_d1 <=  count8;
            count8_d2 <=  count8_d1;
            count8_d3 <=  count8_d2;
            level8_d1 <=  level8;
            sticky_high_8_d1 <=  sticky_high_8;
            sticky_high_8_d2 <=  sticky_high_8_d1;
            sticky_low_8_d1 <=  sticky_low_8;
            sticky_low_8_d2 <=  sticky_low_8_d1;
            sticky_low_8_d3 <=  sticky_low_8_d2;
            sticky_low_8_d4 <=  sticky_low_8_d3;
            sticky8_d1 <=  sticky8;
            count7_d1 <=  count7;
            count7_d2 <=  count7_d1;
            level7_d1 <=  level7;
            sticky_high_7_d1 <=  sticky_high_7;
            sticky_low_7_d1 <=  sticky_low_7;
            sticky7_d1 <=  sticky7;
            count6_d1 <=  count6;
            sticky_high_6_d1 <=  sticky_high_6;
            sticky_low_6_d1 <=  sticky_low_6;
            count5_d1 <=  count5;
            level5_d1 <=  level5;
            sticky5_d1 <=  sticky5;
            sticky_high_4_d1 <=  sticky_high_4;
            sticky_low_4_d1 <=  sticky_low_4;
            sticky_low_2_d1 <=  sticky_low_2;
            sticky_low_2_d2 <=  sticky_low_2_d1;
            sticky_low_2_d3 <=  sticky_low_2_d2;
            sticky_low_2_d4 <=  sticky_low_2_d3;
            sticky_low_2_d5 <=  sticky_low_2_d4;
            sticky_low_2_d6 <=  sticky_low_2_d5;
            sticky_low_2_d7 <=  sticky_low_2_d6;
            sticky_low_1_d1 <=  sticky_low_1;
            sticky_low_1_d2 <=  sticky_low_1_d1;
            sticky_low_1_d3 <=  sticky_low_1_d2;
            sticky_low_1_d4 <=  sticky_low_1_d3;
            sticky_low_1_d5 <=  sticky_low_1_d4;
            sticky_low_1_d6 <=  sticky_low_1_d5;
            sticky_low_1_d7 <=  sticky_low_1_d6;
            sticky_low_0_d1 <=  sticky_low_0;
            sticky_low_0_d2 <=  sticky_low_0_d1;
            sticky_low_0_d3 <=  sticky_low_0_d2;
            sticky_low_0_d4 <=  sticky_low_0_d3;
            sticky_low_0_d5 <=  sticky_low_0_d4;
            sticky_low_0_d6 <=  sticky_low_0_d5;
            sticky_low_0_d7 <=  sticky_low_0_d6;
         end if;
      end process;
   level10 <= I ;
   sozb<= OZb;
   sticky10 <= '0' ;
   count9<= '1' when level10_d2(527 downto 16) = (527 downto 16=>sozb_d2) else '0';
   level9<= level10_d2(527 downto 0) when count9='0' else level10_d2(15 downto 0) & (511 downto 0 => '0');
   sticky_high_9<= '0';
   sticky_low_9<= '0';
   sticky9<= sticky10_d2 or sticky_high_9_d2 when count9='0' else sticky10_d2 or sticky_low_9_d2;

   count8<= '1' when level9_d2(527 downto 272) = (527 downto 272=>sozb_d4) else '0';
   level8<= level9_d2(527 downto 17) when count8='0' else level9_d2(271 downto 0) & (238 downto 0 => '0');
   sticky_high_8<= '0'when level9(16 downto 0) = CONV_STD_LOGIC_VECTOR(0,17) else '1';
   sticky_low_8<= '0';
   sticky8<= sticky9_d2 or sticky_high_8_d2 when count8='0' else sticky9_d2 or sticky_low_8_d4;

   count7<= '1' when level8_d1(510 downto 383) = (510 downto 383=>sozb_d5) else '0';
   level7<= level8_d1(510 downto 256) when count7='0' else level8_d1(382 downto 128);
   sticky_high_7<= '0'when level8(255 downto 0) = CONV_STD_LOGIC_VECTOR(0,256) else '1';
   sticky_low_7<= '0'when level8(127 downto 0) = CONV_STD_LOGIC_VECTOR(0,128) else '1';
   sticky7<= sticky8_d1 or sticky_high_7_d1 when count7='0' else sticky8_d1 or sticky_low_7_d1;

   count6<= '1' when level7_d1(254 downto 191) = (254 downto 191=>sozb_d6) else '0';
   level6<= level7_d1(254 downto 128) when count6='0' else level7_d1(190 downto 64);
   sticky_high_6<= '0'when level7(127 downto 0) = CONV_STD_LOGIC_VECTOR(0,128) else '1';
   sticky_low_6<= '0'when level7(63 downto 0) = CONV_STD_LOGIC_VECTOR(0,64) else '1';
   sticky6<= sticky7_d1 or sticky_high_6_d1 when count6='0' else sticky7_d1 or sticky_low_6_d1;

   count5<= '1' when level6(126 downto 95) = (126 downto 95=>sozb_d6) else '0';
   level5<= level6(126 downto 64) when count5='0' else level6(94 downto 32);
   sticky_high_5<= '0'when level6(63 downto 0) = CONV_STD_LOGIC_VECTOR(0,64) else '1';
   sticky_low_5<= '0'when level6(31 downto 0) = CONV_STD_LOGIC_VECTOR(0,32) else '1';
   sticky5<= sticky6 or sticky_high_5 when count5='0' else sticky6 or sticky_low_5;

   count4<= '1' when level5_d1(62 downto 47) = (62 downto 47=>sozb_d7) else '0';
   level4<= level5_d1(62 downto 32) when count4='0' else level5_d1(46 downto 16);
   sticky_high_4<= '0'when level5(31 downto 0) = CONV_STD_LOGIC_VECTOR(0,32) else '1';
   sticky_low_4<= '0'when level5(15 downto 0) = CONV_STD_LOGIC_VECTOR(0,16) else '1';
   sticky4<= sticky5_d1 or sticky_high_4_d1 when count4='0' else sticky5_d1 or sticky_low_4_d1;

   count3<= '1' when level4(30 downto 23) = (30 downto 23=>sozb_d7) else '0';
   level3<= level4(30 downto 15) when count3='0' else level4(22 downto 7);
   sticky_high_3<= '0'when level4(14 downto 0) = CONV_STD_LOGIC_VECTOR(0,15) else '1';
   sticky_low_3<= '0'when level4(6 downto 0) = CONV_STD_LOGIC_VECTOR(0,7) else '1';
   sticky3<= sticky4 or sticky_high_3 when count3='0' else sticky4 or sticky_low_3;

   count2<= '1' when level3(15 downto 12) = (15 downto 12=>sozb_d7) else '0';
   level2<= level3(15 downto 4) when count2='0' else level3(11 downto 0);
   sticky_high_2<= '0'when level3(3 downto 0) = CONV_STD_LOGIC_VECTOR(0,4) else '1';
   sticky_low_2<= '0';
   sticky2<= sticky3 or sticky_high_2 when count2='0' else sticky3 or sticky_low_2_d7;

   count1<= '1' when level2(11 downto 10) = (11 downto 10=>sozb_d7) else '0';
   level1<= level2(11 downto 2) when count1='0' else level2(9 downto 0);
   sticky_high_1<= '0'when level2(1 downto 0) = CONV_STD_LOGIC_VECTOR(0,2) else '1';
   sticky_low_1<= '0';
   sticky1<= sticky2 or sticky_high_1 when count1='0' else sticky2 or sticky_low_1_d7;

   count0<= '1' when level1(9 downto 9) = (9 downto 9=>sozb_d7) else '0';
   level0<= level1(9 downto 1) when count0='0' else level1(8 downto 0);
   sticky_high_0<= '0'when level1(0 downto 0) = CONV_STD_LOGIC_VECTOR(0,1) else '1';
   sticky_low_0<= '0';
   sticky0<= sticky1 or sticky_high_0 when count0='0' else sticky1 or sticky_low_0_d7;

   O <= level0;
   sCount <= count9_d5 & count8_d3 & count7_d2 & count6_d1 & count5_d1 & count4 & count3 & count2 & count1 & count0;
   Count <= sCount;
   Sticky <= sticky0;
end architecture;

--------------------------------------------------------------------------------
--                     RightShifter9_by_max_9_F250_uid22
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Bogdan Pasca (2008-2011), Florent de Dinechin (2008-2019)
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: X S padBit
-- Output signals: R

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity RightShifter9_by_max_9_F250_uid22 is
    port (clk : in std_logic;
          X : in  std_logic_vector(8 downto 0);
          S : in  std_logic_vector(3 downto 0);
          padBit : in  std_logic;
          R : out  std_logic_vector(17 downto 0)   );
end entity;

architecture arch of RightShifter9_by_max_9_F250_uid22 is
signal ps :  std_logic_vector(3 downto 0);
signal level0, level0_d1 :  std_logic_vector(8 downto 0);
signal level1 :  std_logic_vector(9 downto 0);
signal level2 :  std_logic_vector(11 downto 0);
signal level3 :  std_logic_vector(15 downto 0);
signal level4 :  std_logic_vector(23 downto 0);
signal padBit_d1, padBit_d2, padBit_d3, padBit_d4, padBit_d5, padBit_d6, padBit_d7, padBit_d8 :  std_logic;
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            level0_d1 <=  level0;
            padBit_d1 <=  padBit;
            padBit_d2 <=  padBit_d1;
            padBit_d3 <=  padBit_d2;
            padBit_d4 <=  padBit_d3;
            padBit_d5 <=  padBit_d4;
            padBit_d6 <=  padBit_d5;
            padBit_d7 <=  padBit_d6;
            padBit_d8 <=  padBit_d7;
         end if;
      end process;
   ps<= S;
   level0<= X;
   level1 <=  (0 downto 0 => padBit_d8) & level0_d1 when ps(0) = '1' else    level0_d1 & (0 downto 0 => '0');
   R <= level4(23 downto 6);
   level2 <=  (1 downto 0 => padBit_d8) & level1 when ps(1) = '1' else    level1 & (1 downto 0 => '0');
   R <= level4(23 downto 6);
   level3 <=  (3 downto 0 => padBit_d8) & level2 when ps(2) = '1' else    level2 & (3 downto 0 => '0');
   R <= level4(23 downto 6);
   level4 <=  (7 downto 0 => padBit_d8) & level3 when ps(3) = '1' else    level3 & (7 downto 0 => '0');
   R <= level4(23 downto 6);
end architecture;

--------------------------------------------------------------------------------
--                                    l2a
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 8 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: A isNaN
-- Output signals: arith_o

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity l2a is
    port (clk : in std_logic;
          A : in  std_logic_vector(527 downto 0);
          isNaN : in  std_logic;
          arith_o : out  std_logic_vector(15 downto 0)   );
end entity;

architecture arch of l2a is
   component LZOCShifterSticky_528_to_9_counting_1024_F250_uid20 is
      port ( clk : in std_logic;
             I : in  std_logic_vector(527 downto 0);
             OZb : in  std_logic;
             Count : out  std_logic_vector(9 downto 0);
             O : out  std_logic_vector(8 downto 0);
             Sticky : out  std_logic   );
   end component;

   component RightShifter9_by_max_9_F250_uid22 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(8 downto 0);
             S : in  std_logic_vector(3 downto 0);
             padBit : in  std_logic;
             R : out  std_logic_vector(17 downto 0)   );
   end component;

signal rippled_carry, rippled_carry_d1, rippled_carry_d2, rippled_carry_d3, rippled_carry_d4, rippled_carry_d5, rippled_carry_d6, rippled_carry_d7, rippled_carry_d8 :  std_logic_vector(527 downto 0);
signal count_bit :  std_logic;
signal count_lzoc_o, count_lzoc_o_d1 :  std_logic_vector(9 downto 0);
signal frac_lzoc_o :  std_logic_vector(8 downto 0);
signal sticky_lzoc_o :  std_logic;
signal unbiased_exp, unbiased_exp_d1 :  std_logic_vector(9 downto 0);
signal bias, bias_d1, bias_d2, bias_d3, bias_d4, bias_d5, bias_d6, bias_d7, bias_d8 :  std_logic_vector(9 downto 0);
signal exp_ext :  std_logic_vector(9 downto 0);
signal sign_exp_ext :  std_logic;
signal exp_ovf :  std_logic;
signal pre_sub_exp_udf :  std_logic;
signal pad :  std_logic;
signal shift_amount :  std_logic_vector(3 downto 0);
signal shifted_mantissa :  std_logic_vector(17 downto 0);
signal exp_udf :  std_logic;
signal biased_exp :  std_logic_vector(7 downto 0);
signal not_frac_lzoc :  std_logic_vector(8 downto 0);
signal unrounded_frac :  std_logic_vector(8 downto 0);
signal G :  std_logic;
signal R :  std_logic;
signal S, S_d1 :  std_logic;
signal round_up :  std_logic;
signal rounded_frac :  std_logic_vector(8 downto 0);
signal post_round_ovf :  std_logic;
signal post_rounding_exp :  std_logic_vector(8 downto 0);
signal nan_out :  std_logic;
signal is_zero :  std_logic;
signal final_exp :  std_logic_vector(7 downto 0);
signal isNaN_d1, isNaN_d2, isNaN_d3, isNaN_d4, isNaN_d5, isNaN_d6, isNaN_d7, isNaN_d8 :  std_logic;
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            rippled_carry_d1 <=  rippled_carry;
            rippled_carry_d2 <=  rippled_carry_d1;
            rippled_carry_d3 <=  rippled_carry_d2;
            rippled_carry_d4 <=  rippled_carry_d3;
            rippled_carry_d5 <=  rippled_carry_d4;
            rippled_carry_d6 <=  rippled_carry_d5;
            rippled_carry_d7 <=  rippled_carry_d6;
            rippled_carry_d8 <=  rippled_carry_d7;
            count_lzoc_o_d1 <=  count_lzoc_o;
            unbiased_exp_d1 <=  unbiased_exp;
            bias_d1 <=  bias;
            bias_d2 <=  bias_d1;
            bias_d3 <=  bias_d2;
            bias_d4 <=  bias_d3;
            bias_d5 <=  bias_d4;
            bias_d6 <=  bias_d5;
            bias_d7 <=  bias_d6;
            bias_d8 <=  bias_d7;
            S_d1 <=  S;
            isNaN_d1 <=  isNaN;
            isNaN_d2 <=  isNaN_d1;
            isNaN_d3 <=  isNaN_d2;
            isNaN_d4 <=  isNaN_d3;
            isNaN_d5 <=  isNaN_d4;
            isNaN_d6 <=  isNaN_d5;
            isNaN_d7 <=  isNaN_d6;
            isNaN_d8 <=  isNaN_d7;
         end if;
      end process;

   rippled_carry <= A;

--------------- Count 0/1 while shifting and sticky computation ---------------
   count_bit <= rippled_carry(527);
   lzoc_inst: LZOCShifterSticky_528_to_9_counting_1024_F250_uid20
      port map ( clk  => clk,
                 I => rippled_carry,
                 OZb => count_bit,
                 Count => count_lzoc_o,
                 O => frac_lzoc_o,
                 Sticky => sticky_lzoc_o);

----------- Compute unbiased exponent from msb weigth and lzoc count -----------
   unbiased_exp <= CONV_STD_LOGIC_VECTOR(261,10) - (count_lzoc_o);
   bias <= CONV_STD_LOGIC_VECTOR(127,10);
   exp_ext <= bias_d8 + unbiased_exp_d1;
   sign_exp_ext <= exp_ext(9);
   exp_ovf <= '1' when ( exp_ext(8 downto 8)>"0" and sign_exp_ext='0') else '0';

------------------------------ Subnormals shifter ------------------------------
   pre_sub_exp_udf <= sign_exp_ext;
   pad <= not(rippled_carry(527));
   shift_amount <= "0000" when pre_sub_exp_udf='1' else CONV_STD_LOGIC_VECTOR(-signed(exp_ext),4);
   rshift_mantissa_inst: RightShifter9_by_max_9_F250_uid22
      port map ( clk  => clk,
                 S => shift_amount,
                 X => frac_lzoc_o,
                 padBit => pad,
                 R => shifted_mantissa);

   exp_udf <= '1' when (shift_amount > 9) else '0';
   biased_exp <= exp_ext(7 downto 0);

-------------------------- Convert in sign magnitude --------------------------
   not_frac_lzoc <=  shifted_mantissa(17 downto 9) xor (8 downto 0 => rippled_carry_d8(527));
   unrounded_frac <= "0" & not_frac_lzoc(7 downto 0) + rippled_carry_d8(527);

---- G and R should be taken from lzoc adding one size more frac lzoc width ----
------------------------------- GRS rounding up -------------------------------
   G <= unrounded_frac(1);
   R <= unrounded_frac(0);
   S <= sticky_lzoc_o;
   round_up <= G and (R or S_d1);
   rounded_frac <= unrounded_frac + round_up;
   post_round_ovf <= rounded_frac(8);

------------------------- post rounding scale handling -------------------------
   post_rounding_exp <= ("0" & biased_exp) + (rounded_frac(8));
   nan_out <= post_rounding_exp(8) or isNaN_d8 or exp_udf or exp_ovf;
is_zero <= count_lzoc_o_d1(9) when rounded_frac="000000000" else '0';
   final_exp <= post_rounding_exp(7 downto 0) when nan_out = '0' else "11111111";
   arith_o <= (rippled_carry_d8(527) & final_exp(7 downto 0) & rounded_frac(7 downto 1)) when is_zero = '0' else "0000000000000000";
end architecture;

--------------------------------------------------------------------------------
--                          DSPBlock_8x8_F250_uid33
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: 
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: X Y
-- Output signals: R

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library std;
use std.textio.all;
library work;

entity DSPBlock_8x8_F250_uid33 is
    port (clk : in std_logic;
          X : in  std_logic_vector(7 downto 0);
          Y : in  std_logic_vector(7 downto 0);
          R : out  std_logic_vector(15 downto 0)   );
end entity;

architecture arch of DSPBlock_8x8_F250_uid33 is
signal Mint :  std_logic_vector(15 downto 0);
signal M :  std_logic_vector(15 downto 0);
signal Rtmp :  std_logic_vector(15 downto 0);
begin
   Mint <= std_logic_vector(unsigned(X) * unsigned(Y)); -- multiplier
   M <= Mint(15 downto 0);
   Rtmp <= M;
   R <= Rtmp;
end architecture;

--------------------------------------------------------------------------------
--                          IntMultiplier_F250_uid29
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Martin Kumm, Florent de Dinechin, Kinga Illyes, Bogdan Popa, Bogdan Pasca, 2012
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: X Y
-- Output signals: R

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library std;
use std.textio.all;
library work;

entity IntMultiplier_F250_uid29 is
    port (clk : in std_logic;
          X : in  std_logic_vector(7 downto 0);
          Y : in  std_logic_vector(7 downto 0);
          R : out  std_logic_vector(15 downto 0)   );
end entity;

architecture arch of IntMultiplier_F250_uid29 is
   component DSPBlock_8x8_F250_uid33 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(7 downto 0);
             Y : in  std_logic_vector(7 downto 0);
             R : out  std_logic_vector(15 downto 0)   );
   end component;

signal XX_m30 :  std_logic_vector(7 downto 0);
signal YY_m30 :  std_logic_vector(7 downto 0);
signal tile_0_X :  std_logic_vector(7 downto 0);
signal tile_0_Y :  std_logic_vector(7 downto 0);
signal tile_0_output :  std_logic_vector(15 downto 0);
signal tile_0_filtered_output :  std_logic_vector(15 downto 0);
signal bh31_w0_0 :  std_logic;
signal bh31_w1_0 :  std_logic;
signal bh31_w2_0 :  std_logic;
signal bh31_w3_0 :  std_logic;
signal bh31_w4_0 :  std_logic;
signal bh31_w5_0 :  std_logic;
signal bh31_w6_0 :  std_logic;
signal bh31_w7_0 :  std_logic;
signal bh31_w8_0 :  std_logic;
signal bh31_w9_0 :  std_logic;
signal bh31_w10_0 :  std_logic;
signal bh31_w11_0 :  std_logic;
signal bh31_w12_0 :  std_logic;
signal bh31_w13_0 :  std_logic;
signal bh31_w14_0 :  std_logic;
signal bh31_w15_0 :  std_logic;
signal tmp_bitheapResult_bh31_15 :  std_logic_vector(15 downto 0);
signal bitheapResult_bh31 :  std_logic_vector(15 downto 0);
begin
   XX_m30 <= X ;
   YY_m30 <= Y ;
   tile_0_X <= X(7 downto 0);
   tile_0_Y <= Y(7 downto 0);
   tile_0_mult: DSPBlock_8x8_F250_uid33
      port map ( clk  => clk,
                 X => tile_0_X,
                 Y => tile_0_Y,
                 R => tile_0_output);

tile_0_filtered_output <= tile_0_output(15 downto 0);
   bh31_w0_0 <= tile_0_filtered_output(0);
   bh31_w1_0 <= tile_0_filtered_output(1);
   bh31_w2_0 <= tile_0_filtered_output(2);
   bh31_w3_0 <= tile_0_filtered_output(3);
   bh31_w4_0 <= tile_0_filtered_output(4);
   bh31_w5_0 <= tile_0_filtered_output(5);
   bh31_w6_0 <= tile_0_filtered_output(6);
   bh31_w7_0 <= tile_0_filtered_output(7);
   bh31_w8_0 <= tile_0_filtered_output(8);
   bh31_w9_0 <= tile_0_filtered_output(9);
   bh31_w10_0 <= tile_0_filtered_output(10);
   bh31_w11_0 <= tile_0_filtered_output(11);
   bh31_w12_0 <= tile_0_filtered_output(12);
   bh31_w13_0 <= tile_0_filtered_output(13);
   bh31_w14_0 <= tile_0_filtered_output(14);
   bh31_w15_0 <= tile_0_filtered_output(15);

   -- Adding the constant bits
      -- All the constant bits are zero, nothing to add

   tmp_bitheapResult_bh31_15 <= bh31_w15_0 & bh31_w14_0 & bh31_w13_0 & bh31_w12_0 & bh31_w11_0 & bh31_w10_0 & bh31_w9_0 & bh31_w8_0 & bh31_w7_0 & bh31_w6_0 & bh31_w5_0 & bh31_w4_0 & bh31_w3_0 & bh31_w2_0 & bh31_w1_0 & bh31_w0_0;
   bitheapResult_bh31 <= tmp_bitheapResult_bh31_15;
   R <= bitheapResult_bh31(15 downto 0);
end architecture;

--------------------------------------------------------------------------------
--                    LeftShifter16_by_max_511_F250_uid36
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Bogdan Pasca (2008-2011), Florent de Dinechin (2008-2019)
--------------------------------------------------------------------------------
-- Pipeline depth: 2 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: X S padBit
-- Output signals: R

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity LeftShifter16_by_max_511_F250_uid36 is
    port (clk : in std_logic;
          X : in  std_logic_vector(15 downto 0);
          S : in  std_logic_vector(8 downto 0);
          padBit : in  std_logic;
          R : out  std_logic_vector(526 downto 0)   );
end entity;

architecture arch of LeftShifter16_by_max_511_F250_uid36 is
signal ps, ps_d1, ps_d2 :  std_logic_vector(8 downto 0);
signal level0 :  std_logic_vector(15 downto 0);
signal level1 :  std_logic_vector(16 downto 0);
signal level2 :  std_logic_vector(18 downto 0);
signal level3 :  std_logic_vector(22 downto 0);
signal level4 :  std_logic_vector(30 downto 0);
signal level5 :  std_logic_vector(46 downto 0);
signal level6 :  std_logic_vector(78 downto 0);
signal level7, level7_d1, level7_d2 :  std_logic_vector(142 downto 0);
signal level8 :  std_logic_vector(270 downto 0);
signal level9 :  std_logic_vector(526 downto 0);
signal padBit_d1, padBit_d2 :  std_logic;
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            ps_d1 <=  ps;
            ps_d2 <=  ps_d1;
            level7_d1 <=  level7;
            level7_d2 <=  level7_d1;
            padBit_d1 <=  padBit;
            padBit_d2 <=  padBit_d1;
         end if;
      end process;
   ps<= S;
   level0<= X;
   level1<= level0 & (0 downto 0 => '0') when ps(0)= '1' else     (0 downto 0 => padBit) & level0;
   R <= level9(526 downto 0);
   level2<= level1 & (1 downto 0 => '0') when ps(1)= '1' else     (1 downto 0 => padBit) & level1;
   R <= level9(526 downto 0);
   level3<= level2 & (3 downto 0 => '0') when ps(2)= '1' else     (3 downto 0 => padBit) & level2;
   R <= level9(526 downto 0);
   level4<= level3 & (7 downto 0 => '0') when ps(3)= '1' else     (7 downto 0 => padBit) & level3;
   R <= level9(526 downto 0);
   level5<= level4 & (15 downto 0 => '0') when ps(4)= '1' else     (15 downto 0 => padBit) & level4;
   R <= level9(526 downto 0);
   level6<= level5 & (31 downto 0 => '0') when ps(5)= '1' else     (31 downto 0 => padBit) & level5;
   R <= level9(526 downto 0);
   level7<= level6 & (63 downto 0 => '0') when ps(6)= '1' else     (63 downto 0 => padBit) & level6;
   R <= level9(526 downto 0);
   level8<= level7_d2 & (127 downto 0 => '0') when ps_d2(7)= '1' else     (127 downto 0 => padBit_d2) & level7_d2;
   R <= level9(526 downto 0);
   level9<= level8 & (255 downto 0 => '0') when ps_d2(8)= '1' else     (255 downto 0 => padBit_d2) & level8;
   R <= level9(526 downto 0);
end architecture;

--------------------------------------------------------------------------------
--                                   s3fdp
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 3 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: S3_x S3_y FTZ EOB
-- Output signals: A EOB_Q isNaN

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity s3fdp is
    port (clk, rst : in std_logic;
          S3_x : in  std_logic_vector(17 downto 0);
          S3_y : in  std_logic_vector(17 downto 0);
          FTZ : in  std_logic;
          EOB : in  std_logic;
          A : out  std_logic_vector(527 downto 0);
          EOB_Q : out  std_logic;
          isNaN : out  std_logic   );
end entity;

architecture arch of s3fdp is
   component IntMultiplier_F250_uid29 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(7 downto 0);
             Y : in  std_logic_vector(7 downto 0);
             R : out  std_logic_vector(15 downto 0)   );
   end component;

   component LeftShifter16_by_max_511_F250_uid36 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(15 downto 0);
             S : in  std_logic_vector(8 downto 0);
             padBit : in  std_logic;
             R : out  std_logic_vector(526 downto 0)   );
   end component;

signal sign_X :  std_logic;
signal sign_Y :  std_logic;
signal sign_M, sign_M_d1, sign_M_d2 :  std_logic;
signal isNaN_X :  std_logic;
signal isNaN_Y :  std_logic;
signal isNaN_M, isNaN_M_d1, isNaN_M_d2 :  std_logic;
signal significand_X :  std_logic_vector(7 downto 0);
signal significand_Y :  std_logic_vector(7 downto 0);
signal significand_product :  std_logic_vector(15 downto 0);
signal scale_X_biased :  std_logic_vector(7 downto 0);
signal scale_Y_biased :  std_logic_vector(7 downto 0);
signal scale_product_twice_biased :  std_logic_vector(8 downto 0);
signal significand_product_cpt1 :  std_logic_vector(15 downto 0);
signal shift_value :  std_logic_vector(8 downto 0);
signal shifted_significand :  std_logic_vector(526 downto 0);
signal too_small, too_small_d1, too_small_d2 :  std_logic;
signal too_big, too_big_d1, too_big_d2 :  std_logic;
signal summand1c :  std_logic_vector(511 downto 0);
signal ext_summand1c :  std_logic_vector(527 downto 0);
signal not_ftz, not_ftz_d1, not_ftz_d2 :  std_logic;
signal EOB_internal, EOB_internal_d1, EOB_internal_d2, EOB_internal_d3 :  std_logic;
signal not_ftz_sync :  std_logic;
signal carry_0_sync :  std_logic;
signal EOB_internal_delayed :  std_logic;
signal isNaN_M_sync :  std_logic;
signal too_big_sync :  std_logic;
signal isNaN_o, isNaN_o_d1 :  std_logic;
signal isNaN_delayed :  std_logic;
signal carry_0 :  std_logic;
signal summand_0 :  std_logic_vector(527 downto 0);
signal summand_and_carry_0 :  std_logic_vector(528 downto 0);
signal acc_0, acc_0_d1 :  std_logic_vector(528 downto 0);
signal acc_0_q :  std_logic_vector(528 downto 0);
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            sign_M_d1 <=  sign_M;
            sign_M_d2 <=  sign_M_d1;
            isNaN_M_d1 <=  isNaN_M;
            isNaN_M_d2 <=  isNaN_M_d1;
            too_small_d1 <=  too_small;
            too_small_d2 <=  too_small_d1;
            too_big_d1 <=  too_big;
            too_big_d2 <=  too_big_d1;
            not_ftz_d1 <=  not_ftz;
            not_ftz_d2 <=  not_ftz_d1;
            EOB_internal_d1 <=  EOB_internal;
            EOB_internal_d2 <=  EOB_internal_d1;
            EOB_internal_d3 <=  EOB_internal_d2;
         end if;
      end process;
   process(clk, rst)
      begin
         if rst = '1' then
            isNaN_o_d1 <=  '0';
            acc_0_d1 <=  (others => '0');
         elsif clk'event and clk = '1' then
            isNaN_o_d1 <=  isNaN_o;
            acc_0_d1 <=  acc_0;
         end if;
      end process;
--------------------------- sign product processing ---------------------------
   sign_X <= S3_x(16);
   sign_Y <= S3_y(16);
   sign_M <= sign_X xor sign_Y;

---------------------------- NaN product processing ----------------------------
   isNaN_X <= S3_x(17);
   isNaN_Y <= S3_y(17);
   isNaN_M <= isNaN_X or isNaN_Y;

---------------------------- significand processing ----------------------------
   significand_X <= S3_x(15 downto 8);
   significand_Y <= S3_y(15 downto 8);
   significand_product_inst: IntMultiplier_F250_uid29
      port map ( clk  => clk,
                 X => significand_X,
                 Y => significand_Y,
                 R => significand_product);

------------------------------- scale processing -------------------------------
   scale_X_biased <= S3_x(7 downto 0);
   scale_Y_biased <= S3_y(7 downto 0);
   scale_product_twice_biased <= ("0" & scale_X_biased) + ("0" & scale_Y_biased);

--------------------------- pre-shift xoring (cpt1) ---------------------------
   significand_product_cpt1 <= significand_product when sign_M='0' else not(significand_product);

------------------------- significand product shifting -------------------------
   shift_value <= (scale_product_twice_biased) - (-13);
   significand_product_shifter_inst: LeftShifter16_by_max_511_F250_uid36
      port map ( clk  => clk,
                 S => shift_value,
                 X => significand_product_cpt1,
                 padBit => sign_M,
                 R => shifted_significand);

-------------- detect too low scale for this specific scratchpad --------------
   too_small <= '1' when (shift_value(8)='1') else '0';

-------------- detect too big scale for this specific scratchpad --------------
   too_big <= '1' when (unsigned(shift_value) > 520 and too_small='0') else '0';

--------------- shifted significand part select to form summand ---------------
   summand1c <= shifted_significand(526 downto 15);
   ext_summand1c <= "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" when too_small_d2='1' else ((527 downto 512 => sign_M_d2) & summand1c);

----------------------------- Syncing some signals -----------------------------
   not_ftz <= not FTZ;
   EOB_internal <= EOB;
   not_ftz_sync <= not_ftz_d2;
   carry_0_sync <= sign_M_d2;
   EOB_internal_delayed <= EOB_internal_d3;
   isNaN_M_sync <= isNaN_M_d2;
   too_big_sync <= too_big_d2;

------------------------------ Output isNaN latch ------------------------------
   isNaN_o <= (too_big_sync or isNaN_M_sync or isNaN_delayed) when not_ftz_sync='1' else '0';
   isNaN_delayed <= isNaN_o_d1;

---------------------------- Carry Save Accumulator ----------------------------
   -- DQ logic
   acc_0_q <= acc_0_d1;

   -- sequential addition logic
   carry_0 <= carry_0_sync;
   summand_0 <= ext_summand1c(527 downto 0);
   summand_and_carry_0 <= ("0" & summand_0) + carry_0;
   acc_0 <= (("0" & acc_0_q(527 downto 0)) + summand_and_carry_0) when (not_ftz_sync='1') else
            summand_and_carry_0;

-------------------------------- Output Compose --------------------------------
   A <= acc_0_q(527 downto 0);

   EOB_Q <= EOB_internal_delayed;
   isNaN <= isNaN_delayed;
end architecture;

--------------------------------------------------------------------------------
--                                   PE_S3
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: s3_row_i_A s3_col_j_B C_out SOB EOB
-- Output signals: s3_row_im1_A s3_col_jm1_B SOB_Q EOB_Q C_out_Q

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity PE_S3 is
    port (clk, rst : in std_logic;
          s3_row_i_A : in  std_logic_vector(17 downto 0);
          s3_col_j_B : in  std_logic_vector(17 downto 0);
          C_out : in  std_logic_vector(528 downto 0);
          SOB : in  std_logic;
          EOB : in  std_logic;
          s3_row_im1_A : out  std_logic_vector(17 downto 0);
          s3_col_jm1_B : out  std_logic_vector(17 downto 0);
          SOB_Q : out  std_logic;
          EOB_Q : out  std_logic;
          C_out_Q : out  std_logic_vector(528 downto 0)   );
end entity;

architecture arch of PE_S3 is
   component s3fdp is
      port ( clk, rst : in std_logic;
             S3_x : in  std_logic_vector(17 downto 0);
             S3_y : in  std_logic_vector(17 downto 0);
             FTZ : in  std_logic;
             EOB : in  std_logic;
             A : out  std_logic_vector(527 downto 0);
             EOB_Q : out  std_logic;
             isNaN : out  std_logic   );
   end component;

signal s3_row_i_A_q :  std_logic_vector(17 downto 0);
signal s3_col_j_B_q :  std_logic_vector(17 downto 0);
signal sob_delayed :  std_logic;
signal eob_delayed :  std_logic;
signal mux_C_out, mux_C_out_d1, mux_C_out_d2 :  std_logic_vector(528 downto 0);
signal mux_C_out_HSSD :  std_logic_vector(528 downto 0);
signal isNaN_s3fdp :  std_logic;
signal EOB_s3fdp :  std_logic;
signal A_s3fdp :  std_logic_vector(527 downto 0);
signal s3_row_i_A_d1 :  std_logic_vector(17 downto 0);
signal s3_col_j_B_d1 :  std_logic_vector(17 downto 0);
signal SOB_d1 :  std_logic;
signal EOB_d1 :  std_logic;
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            mux_C_out_d1 <=  mux_C_out;
            mux_C_out_d2 <=  mux_C_out_d1;
            s3_row_i_A_d1 <=  s3_row_i_A;
            s3_col_j_B_d1 <=  s3_col_j_B;
            SOB_d1 <=  SOB;
            EOB_d1 <=  EOB;
         end if;
      end process;
------------------------ Functional delay z-1 of inputs ------------------------
   s3_row_i_A_q <= s3_row_i_A_d1;
   s3_col_j_B_q <= s3_col_j_B_d1;

------------------------- DQ flip flop for SOB and EOB -------------------------
   sob_delayed <= SOB_d1;
   eob_delayed <= EOB_d1;

----------------------------- Half Speed Sink Down -----------------------------
   with EOB_s3fdp  select  mux_C_out <= 
        (isNaN_s3fdp & A_s3fdp) when '1', 
        C_out when others;
   mux_C_out_HSSD <= mux_C_out_d2;

---------------------------- Instantiates the S3FDP ----------------------------
   s3fdp_inst: s3fdp
      port map ( clk => clk,
                 rst  => rst,
                 EOB => EOB,
                 FTZ => SOB,
                 S3_x => s3_row_i_A,
                 S3_y => s3_col_j_B,
                 A => A_s3fdp,
                 EOB_Q => EOB_s3fdp,
                 isNaN => isNaN_s3fdp);

------------------------- Compose the outputs signals -------------------------
   s3_row_im1_A <= s3_row_i_A_q;
   s3_col_jm1_B <= s3_col_j_B_q;
   SOB_Q <= sob_delayed;
   EOB_Q <= eob_delayed;
   C_out_Q <= mux_C_out_HSSD;
end architecture;

--------------------------------------------------------------------------------
--                            SystolicArrayKernel
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: rowsA colsB SOB EOB
-- Output signals: colsC EOB_Q_o

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity SystolicArrayKernel is
    port (clk, rst : in std_logic;
          rowsA : in  std_logic_vector(143 downto 0);
          colsB : in  std_logic_vector(125 downto 0);
          SOB : in  std_logic;
          EOB : in  std_logic;
          colsC : out  std_logic_vector(3702 downto 0);
          EOB_Q_o : out  std_logic   );
end entity;

architecture arch of SystolicArrayKernel is
   component PE_S3 is
      port ( clk, rst : in std_logic;
             s3_row_i_A : in  std_logic_vector(17 downto 0);
             s3_col_j_B : in  std_logic_vector(17 downto 0);
             C_out : in  std_logic_vector(528 downto 0);
             SOB : in  std_logic;
             EOB : in  std_logic;
             s3_row_im1_A : out  std_logic_vector(17 downto 0);
             s3_col_jm1_B : out  std_logic_vector(17 downto 0);
             SOB_Q : out  std_logic;
             EOB_Q : out  std_logic;
             C_out_Q : out  std_logic_vector(528 downto 0)   );
   end component;

type T_2D_LAICPT2_np1_m is array(8 downto 0, 6 downto 0) of std_logic_vector(528 downto 0);
type T_2D_n_mp1 is array(7 downto 0, 7 downto 0) of std_logic_vector(17 downto 0);
type T_2D_np1_m is array(8 downto 0, 6 downto 0) of std_logic_vector(17 downto 0);
type T_2D_np1_m_logic is array(8 downto 0, 6 downto 0) of std_logic;
signal systolic_wires_rows_2D : T_2D_n_mp1;
signal systolic_wires_cols_2D : T_2D_np1_m;
signal systolic_sob_2D : T_2D_np1_m_logic;
signal systolic_eob_2D : T_2D_np1_m_logic;
signal systolic_C_out_2D : T_2D_LAICPT2_np1_m;
begin

----------------- Connect bus of B columns to top edges SA PEs -----------------
   cols_in: for JJ in 0 to 6 generate
      systolic_wires_cols_2D(0,JJ) <= colsB(((JJ+1)*18)-1 downto (JJ*18));
   end generate;

------------------ Connect bus of A rows to left edges SA PEs ------------------
   rows_in: for II in 0 to 7 generate
      systolic_wires_rows_2D(II,0) <= rowsA(((II+1)*18)-1 downto (II*18));
   end generate;

-------------- Connect the Start of Block signals of the TOP PEs --------------
   systolic_sob_2D(0,0) <= SOB;
   sob_1st_row: for JJ in 1 to 6 generate
      systolic_sob_2D(0,JJ) <= systolic_sob_2D(1,JJ-1);
   end generate;

--------------- Connect the End of Block signals of the TOP PEs ---------------
   systolic_eob_2D(0,0) <= EOB;
   eob_1st_row: for JJ in 1 to 6 generate
      systolic_eob_2D(0,JJ) <= systolic_eob_2D(1,JJ-1);
   end generate;

----------- Connect with 0s the input C carry out scheme of TOP PEs -----------
   C_out_input_1st_row: for JJ in 0 to 6 generate
      systolic_C_out_2D(0,JJ) <= "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
   end generate;

------------------------- Connect PEs locally together -------------------------
   rows: for II in 0 to 7 generate
      cols: for JJ in 0 to 6 generate
         PE_ij: PE_S3
            port map ( clk => clk,
                       rst => rst,
                       s3_row_i_A => systolic_wires_rows_2D(II,JJ),
                       s3_col_j_B => systolic_wires_cols_2D(II,JJ),
                       SOB => systolic_sob_2D(II,JJ),
                       SOB_Q => systolic_sob_2D(II+1,JJ),
                       EOB => systolic_eob_2D(II,JJ),
                       EOB_Q => systolic_eob_2D(II+1,JJ),
                       C_out => systolic_C_out_2D(II,JJ),
                       C_out_Q => systolic_C_out_2D(II+1,JJ),
                       s3_row_im1_A => systolic_wires_rows_2D(II,JJ+1),
                       s3_col_jm1_B => systolic_wires_cols_2D(II+1,JJ));
      end generate;
   end generate;

------------------ Connect last row output C to output C bus ------------------
   cols_C_out: for JJ in 0 to 6 generate
      colsC(((JJ+1)*529)-1 downto (JJ*529)) <= systolic_C_out_2D(8,JJ);
   end generate;

------ Connect PE(N-1,M-1) EOB_Q to out world for valid data computation ------
   EOB_Q_o <= systolic_eob_2D(8,6);

end architecture;

--------------------------------------------------------------------------------
--                               SystolicArray
--                (SA_orthogonal_8w7h_ieee_8_7_HSSD_F250_uid2)
-- VHDL generated for VirtexUltrascalePlus @ 250MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 4
-- Target frequency (MHz): 250
-- Input signals: rowsA colsB SOB EOB
-- Output signals: colsC EOB_Q_o

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity SystolicArray is
    port (clk, rst : in std_logic;
          rowsA : in  std_logic_vector(127 downto 0);
          colsB : in  std_logic_vector(111 downto 0);
          SOB : in  std_logic;
          EOB : in  std_logic;
          colsC : out  std_logic_vector(111 downto 0);
          EOB_Q_o : out  std_logic   );
end entity;

architecture arch of SystolicArray is
   component Arith_to_S3 is
      port ( clk : in std_logic;
             arith_i : in  std_logic_vector(15 downto 0);
             S3_o : out  std_logic_vector(17 downto 0)   );
   end component;

   component l2a is
      port ( clk : in std_logic;
             A : in  std_logic_vector(527 downto 0);
             isNaN : in  std_logic;
             arith_o : out  std_logic_vector(15 downto 0)   );
   end component;

   component SystolicArrayKernel is
      port ( clk, rst : in std_logic;
             rowsA : in  std_logic_vector(143 downto 0);
             colsB : in  std_logic_vector(125 downto 0);
             SOB : in  std_logic;
             EOB : in  std_logic;
             colsC : out  std_logic_vector(3702 downto 0);
             EOB_Q_o : out  std_logic   );
   end component;

type array_M_dense is array(6 downto 0) of std_logic_vector(15 downto 0);
type array_M_s3 is array(6 downto 0) of std_logic_vector(17 downto 0);
type array_N_dense is array(7 downto 0) of std_logic_vector(15 downto 0);
type array_N_s3 is array(7 downto 0) of std_logic_vector(17 downto 0);
signal arith_in_row_0 :  std_logic_vector(15 downto 0);
signal arith_in_row_0_q0 :  std_logic_vector(15 downto 0);
signal arith_in_row_1, arith_in_row_1_d1 :  std_logic_vector(15 downto 0);
signal arith_in_row_1_q1 :  std_logic_vector(15 downto 0);
signal arith_in_row_2, arith_in_row_2_d1, arith_in_row_2_d2 :  std_logic_vector(15 downto 0);
signal arith_in_row_2_q2 :  std_logic_vector(15 downto 0);
signal arith_in_row_3, arith_in_row_3_d1, arith_in_row_3_d2, arith_in_row_3_d3 :  std_logic_vector(15 downto 0);
signal arith_in_row_3_q3 :  std_logic_vector(15 downto 0);
signal arith_in_row_4, arith_in_row_4_d1, arith_in_row_4_d2, arith_in_row_4_d3, arith_in_row_4_d4 :  std_logic_vector(15 downto 0);
signal arith_in_row_4_q4 :  std_logic_vector(15 downto 0);
signal arith_in_row_5, arith_in_row_5_d1, arith_in_row_5_d2, arith_in_row_5_d3, arith_in_row_5_d4, arith_in_row_5_d5 :  std_logic_vector(15 downto 0);
signal arith_in_row_5_q5 :  std_logic_vector(15 downto 0);
signal arith_in_row_6, arith_in_row_6_d1, arith_in_row_6_d2, arith_in_row_6_d3, arith_in_row_6_d4, arith_in_row_6_d5, arith_in_row_6_d6 :  std_logic_vector(15 downto 0);
signal arith_in_row_6_q6 :  std_logic_vector(15 downto 0);
signal arith_in_row_7, arith_in_row_7_d1, arith_in_row_7_d2, arith_in_row_7_d3, arith_in_row_7_d4, arith_in_row_7_d5, arith_in_row_7_d6, arith_in_row_7_d7 :  std_logic_vector(15 downto 0);
signal arith_in_row_7_q7 :  std_logic_vector(15 downto 0);
signal arith_in_col_0 :  std_logic_vector(15 downto 0);
signal arith_in_col_0_q0 :  std_logic_vector(15 downto 0);
signal arith_in_col_1, arith_in_col_1_d1 :  std_logic_vector(15 downto 0);
signal arith_in_col_1_q1 :  std_logic_vector(15 downto 0);
signal arith_in_col_2, arith_in_col_2_d1, arith_in_col_2_d2 :  std_logic_vector(15 downto 0);
signal arith_in_col_2_q2 :  std_logic_vector(15 downto 0);
signal arith_in_col_3, arith_in_col_3_d1, arith_in_col_3_d2, arith_in_col_3_d3 :  std_logic_vector(15 downto 0);
signal arith_in_col_3_q3 :  std_logic_vector(15 downto 0);
signal arith_in_col_4, arith_in_col_4_d1, arith_in_col_4_d2, arith_in_col_4_d3, arith_in_col_4_d4 :  std_logic_vector(15 downto 0);
signal arith_in_col_4_q4 :  std_logic_vector(15 downto 0);
signal arith_in_col_5, arith_in_col_5_d1, arith_in_col_5_d2, arith_in_col_5_d3, arith_in_col_5_d4, arith_in_col_5_d5 :  std_logic_vector(15 downto 0);
signal arith_in_col_5_q5 :  std_logic_vector(15 downto 0);
signal arith_in_col_6, arith_in_col_6_d1, arith_in_col_6_d2, arith_in_col_6_d3, arith_in_col_6_d4, arith_in_col_6_d5, arith_in_col_6_d6 :  std_logic_vector(15 downto 0);
signal arith_in_col_6_q6 :  std_logic_vector(15 downto 0);
signal colsC_LAICPT2 :  std_logic_vector(3702 downto 0);
signal SOB_select :  std_logic;
signal SOB_q0 :  std_logic;
signal EOB_select :  std_logic;
signal EOB_q0 :  std_logic;
signal LAICPT2_to_arith :  std_logic_vector(111 downto 0);
signal arith_out_col_out_0, arith_out_col_out_0_d1, arith_out_col_out_0_d2, arith_out_col_out_0_d3, arith_out_col_out_0_d4, arith_out_col_out_0_d5, arith_out_col_out_0_d6 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_0_q6 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_1, arith_out_col_out_1_d1, arith_out_col_out_1_d2, arith_out_col_out_1_d3, arith_out_col_out_1_d4, arith_out_col_out_1_d5 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_1_q5 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_2, arith_out_col_out_2_d1, arith_out_col_out_2_d2, arith_out_col_out_2_d3, arith_out_col_out_2_d4 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_2_q4 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_3, arith_out_col_out_3_d1, arith_out_col_out_3_d2, arith_out_col_out_3_d3 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_3_q3 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_4, arith_out_col_out_4_d1, arith_out_col_out_4_d2 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_4_q2 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_5, arith_out_col_out_5_d1 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_5_q1 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_6 :  std_logic_vector(15 downto 0);
signal arith_out_col_out_6_q0 :  std_logic_vector(15 downto 0);
signal rows_i_arith : array_N_dense;
signal rows_i_s3 :  std_logic_vector(143 downto 0);
signal cols_j_arith : array_M_dense;
signal cols_j_s3 :  std_logic_vector(125 downto 0);
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            arith_in_row_1_d1 <=  arith_in_row_1;
            arith_in_row_2_d1 <=  arith_in_row_2;
            arith_in_row_2_d2 <=  arith_in_row_2_d1;
            arith_in_row_3_d1 <=  arith_in_row_3;
            arith_in_row_3_d2 <=  arith_in_row_3_d1;
            arith_in_row_3_d3 <=  arith_in_row_3_d2;
            arith_in_row_4_d1 <=  arith_in_row_4;
            arith_in_row_4_d2 <=  arith_in_row_4_d1;
            arith_in_row_4_d3 <=  arith_in_row_4_d2;
            arith_in_row_4_d4 <=  arith_in_row_4_d3;
            arith_in_row_5_d1 <=  arith_in_row_5;
            arith_in_row_5_d2 <=  arith_in_row_5_d1;
            arith_in_row_5_d3 <=  arith_in_row_5_d2;
            arith_in_row_5_d4 <=  arith_in_row_5_d3;
            arith_in_row_5_d5 <=  arith_in_row_5_d4;
            arith_in_row_6_d1 <=  arith_in_row_6;
            arith_in_row_6_d2 <=  arith_in_row_6_d1;
            arith_in_row_6_d3 <=  arith_in_row_6_d2;
            arith_in_row_6_d4 <=  arith_in_row_6_d3;
            arith_in_row_6_d5 <=  arith_in_row_6_d4;
            arith_in_row_6_d6 <=  arith_in_row_6_d5;
            arith_in_row_7_d1 <=  arith_in_row_7;
            arith_in_row_7_d2 <=  arith_in_row_7_d1;
            arith_in_row_7_d3 <=  arith_in_row_7_d2;
            arith_in_row_7_d4 <=  arith_in_row_7_d3;
            arith_in_row_7_d5 <=  arith_in_row_7_d4;
            arith_in_row_7_d6 <=  arith_in_row_7_d5;
            arith_in_row_7_d7 <=  arith_in_row_7_d6;
            arith_in_col_1_d1 <=  arith_in_col_1;
            arith_in_col_2_d1 <=  arith_in_col_2;
            arith_in_col_2_d2 <=  arith_in_col_2_d1;
            arith_in_col_3_d1 <=  arith_in_col_3;
            arith_in_col_3_d2 <=  arith_in_col_3_d1;
            arith_in_col_3_d3 <=  arith_in_col_3_d2;
            arith_in_col_4_d1 <=  arith_in_col_4;
            arith_in_col_4_d2 <=  arith_in_col_4_d1;
            arith_in_col_4_d3 <=  arith_in_col_4_d2;
            arith_in_col_4_d4 <=  arith_in_col_4_d3;
            arith_in_col_5_d1 <=  arith_in_col_5;
            arith_in_col_5_d2 <=  arith_in_col_5_d1;
            arith_in_col_5_d3 <=  arith_in_col_5_d2;
            arith_in_col_5_d4 <=  arith_in_col_5_d3;
            arith_in_col_5_d5 <=  arith_in_col_5_d4;
            arith_in_col_6_d1 <=  arith_in_col_6;
            arith_in_col_6_d2 <=  arith_in_col_6_d1;
            arith_in_col_6_d3 <=  arith_in_col_6_d2;
            arith_in_col_6_d4 <=  arith_in_col_6_d3;
            arith_in_col_6_d5 <=  arith_in_col_6_d4;
            arith_in_col_6_d6 <=  arith_in_col_6_d5;
            arith_out_col_out_0_d1 <=  arith_out_col_out_0;
            arith_out_col_out_0_d2 <=  arith_out_col_out_0_d1;
            arith_out_col_out_0_d3 <=  arith_out_col_out_0_d2;
            arith_out_col_out_0_d4 <=  arith_out_col_out_0_d3;
            arith_out_col_out_0_d5 <=  arith_out_col_out_0_d4;
            arith_out_col_out_0_d6 <=  arith_out_col_out_0_d5;
            arith_out_col_out_1_d1 <=  arith_out_col_out_1;
            arith_out_col_out_1_d2 <=  arith_out_col_out_1_d1;
            arith_out_col_out_1_d3 <=  arith_out_col_out_1_d2;
            arith_out_col_out_1_d4 <=  arith_out_col_out_1_d3;
            arith_out_col_out_1_d5 <=  arith_out_col_out_1_d4;
            arith_out_col_out_2_d1 <=  arith_out_col_out_2;
            arith_out_col_out_2_d2 <=  arith_out_col_out_2_d1;
            arith_out_col_out_2_d3 <=  arith_out_col_out_2_d2;
            arith_out_col_out_2_d4 <=  arith_out_col_out_2_d3;
            arith_out_col_out_3_d1 <=  arith_out_col_out_3;
            arith_out_col_out_3_d2 <=  arith_out_col_out_3_d1;
            arith_out_col_out_3_d3 <=  arith_out_col_out_3_d2;
            arith_out_col_out_4_d1 <=  arith_out_col_out_4;
            arith_out_col_out_4_d2 <=  arith_out_col_out_4_d1;
            arith_out_col_out_5_d1 <=  arith_out_col_out_5;
         end if;
      end process;
------------ Delay depending on row index incoming dense arithmetic ------------
   arith_in_row_0 <= rowsA(15 downto 0);
   arith_in_row_0_q0 <= arith_in_row_0;
   arith_in_row_1 <= rowsA(31 downto 16);
   arith_in_row_1_q1 <= arith_in_row_1_d1;
   arith_in_row_2 <= rowsA(47 downto 32);
   arith_in_row_2_q2 <= arith_in_row_2_d2;
   arith_in_row_3 <= rowsA(63 downto 48);
   arith_in_row_3_q3 <= arith_in_row_3_d3;
   arith_in_row_4 <= rowsA(79 downto 64);
   arith_in_row_4_q4 <= arith_in_row_4_d4;
   arith_in_row_5 <= rowsA(95 downto 80);
   arith_in_row_5_q5 <= arith_in_row_5_d5;
   arith_in_row_6 <= rowsA(111 downto 96);
   arith_in_row_6_q6 <= arith_in_row_6_d6;
   arith_in_row_7 <= rowsA(127 downto 112);
   arith_in_row_7_q7 <= arith_in_row_7_d7;

------------ Delay depending on col index incoming dense arithmetic ------------
   arith_in_col_0 <= colsB(15 downto 0);
   arith_in_col_0_q0 <= arith_in_col_0;
   arith_in_col_1 <= colsB(31 downto 16);
   arith_in_col_1_q1 <= arith_in_col_1_d1;
   arith_in_col_2 <= colsB(47 downto 32);
   arith_in_col_2_q2 <= arith_in_col_2_d2;
   arith_in_col_3 <= colsB(63 downto 48);
   arith_in_col_3_q3 <= arith_in_col_3_d3;
   arith_in_col_4 <= colsB(79 downto 64);
   arith_in_col_4_q4 <= arith_in_col_4_d4;
   arith_in_col_5 <= colsB(95 downto 80);
   arith_in_col_5_q5 <= arith_in_col_5_d5;
   arith_in_col_6 <= colsB(111 downto 96);
   arith_in_col_6_q6 <= arith_in_col_6_d6;

--------------- Delay SOB/EOB with Arith_to_S3 delay to feed SAK ---------------
   SOB_select <= SOB;
   SOB_q0 <= SOB_select;
   EOB_select <= EOB;
   EOB_q0 <= EOB_select;

--------------- Delay outgoing arithmetic depending on col index ---------------
   arith_out_col_out_0 <= LAICPT2_to_arith(15 downto 0);
   arith_out_col_out_0_q6 <= arith_out_col_out_0_d6;
   arith_out_col_out_1 <= LAICPT2_to_arith(31 downto 16);
   arith_out_col_out_1_q5 <= arith_out_col_out_1_d5;
   arith_out_col_out_2 <= LAICPT2_to_arith(47 downto 32);
   arith_out_col_out_2_q4 <= arith_out_col_out_2_d4;
   arith_out_col_out_3 <= LAICPT2_to_arith(63 downto 48);
   arith_out_col_out_3_q3 <= arith_out_col_out_3_d3;
   arith_out_col_out_4 <= LAICPT2_to_arith(79 downto 64);
   arith_out_col_out_4_q2 <= arith_out_col_out_4_d2;
   arith_out_col_out_5 <= LAICPT2_to_arith(95 downto 80);
   arith_out_col_out_5_q1 <= arith_out_col_out_5_d1;
   arith_out_col_out_6 <= LAICPT2_to_arith(111 downto 96);
   arith_out_col_out_6_q0 <= arith_out_col_out_6;

---------------- Generate Arith_to_S3 for rows and connect them ----------------
   rows_i_arith(0) <= arith_in_row_0_q0;
   rows_i_arith(1) <= arith_in_row_1_q1;
   rows_i_arith(2) <= arith_in_row_2_q2;
   rows_i_arith(3) <= arith_in_row_3_q3;
   rows_i_arith(4) <= arith_in_row_4_q4;
   rows_i_arith(5) <= arith_in_row_5_q5;
   rows_i_arith(6) <= arith_in_row_6_q6;
   rows_i_arith(7) <= arith_in_row_7_q7;
   rows_a2s3: for II in 0 to 7 generate
      a2s3_i: Arith_to_S3
         port map ( clk => clk,
                    arith_i => rows_i_arith(II),
                    s3_o => rows_i_s3(((II+1)*18)-1 downto II*18));
   end generate;

---------------- Generate Arith_to_S3 for cols and connect them ----------------
   cols_j_arith(0) <= arith_in_col_0_q0;
   cols_j_arith(1) <= arith_in_col_1_q1;
   cols_j_arith(2) <= arith_in_col_2_q2;
   cols_j_arith(3) <= arith_in_col_3_q3;
   cols_j_arith(4) <= arith_in_col_4_q4;
   cols_j_arith(5) <= arith_in_col_5_q5;
   cols_j_arith(6) <= arith_in_col_6_q6;
   cols_a2s3: for JJ in 0 to 6 generate
      a2s3_j: Arith_to_S3
         port map ( clk => clk,
                    arith_i => cols_j_arith(JJ),
                    s3_o => cols_j_s3(((JJ+1)*18)-1 downto JJ*18));
   end generate;

-------------------- Instantiate the Systolic Array Kernel --------------------
   sak: SystolicArrayKernel
      port map ( clk => clk,
                 rst => rst,
                 rowsA => rows_i_s3,
                 colsB => cols_j_s3,
                 SOB => SOB_q0,
                 EOB => EOB_q0,
                 EOB_Q_o => EOB_Q_o,
                 colsC => colsC_LAICPT2 );

-------------------------- Generate LAICPT2_to_arith --------------------------
   cols_l2a: for JJ in 0 to 6 generate
      l2a_i: l2a
         port map ( clk => clk,
                    A => colsC_LAICPT2(((JJ+1)*529)-1-1-0 downto JJ*529),
                    isNaN => colsC_LAICPT2(((JJ+1)*529)- 1),
                    arith_o => LAICPT2_to_arith(((JJ+1)*16)-1 downto JJ*16));
   end generate;

-------- Connect outgoing delayed dense arith words to colsC output bus --------
   colsC(15 downto 0) <= arith_out_col_out_0_q6;
   colsC(31 downto 16) <= arith_out_col_out_1_q5;
   colsC(47 downto 32) <= arith_out_col_out_2_q4;
   colsC(63 downto 48) <= arith_out_col_out_3_q3;
   colsC(79 downto 64) <= arith_out_col_out_4_q2;
   colsC(95 downto 80) <= arith_out_col_out_5_q1;
   colsC(111 downto 96) <= arith_out_col_out_6_q0;

end architecture;

