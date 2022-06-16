--------------------------------------------------------------------------------
--                                Arith_to_S3
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: BSC / UPC - Ledoux Louis
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
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
          arith_i : in  std_logic_vector(31 downto 0);
          S3_o : out  std_logic_vector(33 downto 0)   );
end entity;

architecture arch of Arith_to_S3 is
signal sign :  std_logic;
signal exponent :  std_logic_vector(7 downto 0);
signal fraction :  std_logic_vector(22 downto 0);
signal isNaN :  std_logic;
signal isExpSubnormalZero :  std_logic;
signal implicit :  std_logic;
signal final_scale :  std_logic_vector(7 downto 0);
begin
   sign <= arith_i(31);
   exponent <= arith_i(30 downto 23);
   fraction <= arith_i(22 downto 0);
   isNaN <= '1' when exponent="11111111" else '0';
   isExpSubnormalZero <= '1' when exponent="00000000" else '0';
   implicit <= not(isExpSubnormalZero);
   final_scale<= "00000001" when isExpSubnormalZero= '1' else  exponent;
   S3_o <= isNaN & sign & implicit & fraction & final_scale;
end architecture;

--------------------------------------------------------------------------------
--             LZOCShifterSticky_32_to_25_counting_64_F200_uid24
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Florent de Dinechin, Bogdan Pasca (2007-2016)
--------------------------------------------------------------------------------
-- Pipeline depth: 1 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
-- Input signals: I OZb
-- Output signals: Count O Sticky

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity LZOCShifterSticky_32_to_25_counting_64_F200_uid24 is
    port (clk : in std_logic;
          I : in  std_logic_vector(31 downto 0);
          OZb : in  std_logic;
          Count : out  std_logic_vector(5 downto 0);
          O : out  std_logic_vector(24 downto 0);
          Sticky : out  std_logic   );
end entity;

architecture arch of LZOCShifterSticky_32_to_25_counting_64_F200_uid24 is
signal level6 :  std_logic_vector(31 downto 0);
signal sozb, sozb_d1 :  std_logic;
signal sticky6 :  std_logic;
signal count5, count5_d1 :  std_logic;
signal level5 :  std_logic_vector(31 downto 0);
signal sticky_high_5 :  std_logic;
signal sticky_low_5 :  std_logic;
signal sticky5 :  std_logic;
signal count4, count4_d1 :  std_logic;
signal level4 :  std_logic_vector(31 downto 0);
signal sticky_high_4 :  std_logic;
signal sticky_low_4 :  std_logic;
signal sticky4 :  std_logic;
signal count3, count3_d1 :  std_logic;
signal level3 :  std_logic_vector(31 downto 0);
signal sticky_high_3 :  std_logic;
signal sticky_low_3 :  std_logic;
signal sticky3 :  std_logic;
signal count2, count2_d1 :  std_logic;
signal level2, level2_d1 :  std_logic_vector(27 downto 0);
signal sticky_high_2 :  std_logic;
signal sticky_low_2 :  std_logic;
signal sticky2, sticky2_d1 :  std_logic;
signal count1 :  std_logic;
signal level1 :  std_logic_vector(25 downto 0);
signal sticky_high_1, sticky_high_1_d1 :  std_logic;
signal sticky_low_1, sticky_low_1_d1 :  std_logic;
signal sticky1 :  std_logic;
signal count0 :  std_logic;
signal level0 :  std_logic_vector(24 downto 0);
signal sticky_high_0 :  std_logic;
signal sticky_low_0, sticky_low_0_d1 :  std_logic;
signal sticky0 :  std_logic;
signal sCount :  std_logic_vector(5 downto 0);
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            sozb_d1 <=  sozb;
            count5_d1 <=  count5;
            count4_d1 <=  count4;
            count3_d1 <=  count3;
            count2_d1 <=  count2;
            level2_d1 <=  level2;
            sticky2_d1 <=  sticky2;
            sticky_high_1_d1 <=  sticky_high_1;
            sticky_low_1_d1 <=  sticky_low_1;
            sticky_low_0_d1 <=  sticky_low_0;
         end if;
      end process;
   level6 <= I ;
   sozb<= OZb;
   sticky6 <= '0' ;
   count5<= '1' when level6(31 downto 0) = (31 downto 0=>sozb) else '0';
   level5<= level6(31 downto 0) when count5='0' else (31 downto 0 => '0');
   sticky_high_5<= '0';
   sticky_low_5<= '0';
   sticky5<= sticky6 or sticky_high_5 when count5='0' else sticky6 or sticky_low_5;

   count4<= '1' when level5(31 downto 16) = (31 downto 16=>sozb) else '0';
   level4<= level5(31 downto 0) when count4='0' else level5(15 downto 0) & (15 downto 0 => '0');
   sticky_high_4<= '0';
   sticky_low_4<= '0';
   sticky4<= sticky5 or sticky_high_4 when count4='0' else sticky5 or sticky_low_4;

   count3<= '1' when level4(31 downto 24) = (31 downto 24=>sozb) else '0';
   level3<= level4(31 downto 0) when count3='0' else level4(23 downto 0) & (7 downto 0 => '0');
   sticky_high_3<= '0';
   sticky_low_3<= '0';
   sticky3<= sticky4 or sticky_high_3 when count3='0' else sticky4 or sticky_low_3;

   count2<= '1' when level3(31 downto 28) = (31 downto 28=>sozb) else '0';
   level2<= level3(31 downto 4) when count2='0' else level3(27 downto 0);
   sticky_high_2<= '0'when level3(3 downto 0) = CONV_STD_LOGIC_VECTOR(0,4) else '1';
   sticky_low_2<= '0';
   sticky2<= sticky3 or sticky_high_2 when count2='0' else sticky3 or sticky_low_2;

   count1<= '1' when level2_d1(27 downto 26) = (27 downto 26=>sozb_d1) else '0';
   level1<= level2_d1(27 downto 2) when count1='0' else level2_d1(25 downto 0);
   sticky_high_1<= '0'when level2(1 downto 0) = CONV_STD_LOGIC_VECTOR(0,2) else '1';
   sticky_low_1<= '0';
   sticky1<= sticky2_d1 or sticky_high_1_d1 when count1='0' else sticky2_d1 or sticky_low_1_d1;

   count0<= '1' when level1(25 downto 25) = (25 downto 25=>sozb_d1) else '0';
   level0<= level1(25 downto 1) when count0='0' else level1(24 downto 0);
   sticky_high_0<= '0'when level1(0 downto 0) = CONV_STD_LOGIC_VECTOR(0,1) else '1';
   sticky_low_0<= '0';
   sticky0<= sticky1 or sticky_high_0 when count0='0' else sticky1 or sticky_low_0_d1;

   O <= level0;
   sCount <= count5_d1 & count4_d1 & count3_d1 & count2_d1 & count1 & count0;
   Count <= sCount;
   Sticky <= sticky0;
end architecture;

--------------------------------------------------------------------------------
--                                    l2a
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 1 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
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
          A : in  std_logic_vector(31 downto 0);
          isNaN : in  std_logic;
          arith_o : out  std_logic_vector(31 downto 0)   );
end entity;

architecture arch of l2a is
   component LZOCShifterSticky_32_to_25_counting_64_F200_uid24 is
      port ( clk : in std_logic;
             I : in  std_logic_vector(31 downto 0);
             OZb : in  std_logic;
             Count : out  std_logic_vector(5 downto 0);
             O : out  std_logic_vector(24 downto 0);
             Sticky : out  std_logic   );
   end component;

signal rippled_carry, rippled_carry_d1 :  std_logic_vector(31 downto 0);
signal count_bit :  std_logic;
signal count_lzoc_o :  std_logic_vector(5 downto 0);
signal frac_lzoc_o :  std_logic_vector(24 downto 0);
signal sticky_lzoc_o :  std_logic;
signal unbiased_exp :  std_logic_vector(5 downto 0);
signal bias, bias_d1 :  std_logic_vector(7 downto 0);
signal biased_exp :  std_logic_vector(7 downto 0);
signal not_frac_lzoc :  std_logic_vector(24 downto 0);
signal unrounded_frac :  std_logic_vector(24 downto 0);
signal G :  std_logic;
signal R :  std_logic;
signal S :  std_logic;
signal round_up :  std_logic;
signal rounded_frac :  std_logic_vector(24 downto 0);
signal post_round_ovf :  std_logic;
signal post_rounding_exp :  std_logic_vector(8 downto 0);
signal nan_out :  std_logic;
signal is_zero :  std_logic;
signal final_exp :  std_logic_vector(7 downto 0);
signal isNaN_d1 :  std_logic;
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            rippled_carry_d1 <=  rippled_carry;
            bias_d1 <=  bias;
            isNaN_d1 <=  isNaN;
         end if;
      end process;

   rippled_carry <= A;

--------------- Count 0/1 while shifting and sticky computation ---------------
   count_bit <= rippled_carry(31);
   lzoc_inst: LZOCShifterSticky_32_to_25_counting_64_F200_uid24
      port map ( clk  => clk,
                 I => rippled_carry,
                 OZb => count_bit,
                 Count => count_lzoc_o,
                 O => frac_lzoc_o,
                 Sticky => sticky_lzoc_o);

----------- Compute unbiased exponent from msb weigth and lzoc count -----------
   unbiased_exp <= CONV_STD_LOGIC_VECTOR(16,6) - (count_lzoc_o);
   bias <= CONV_STD_LOGIC_VECTOR(127,8);
   biased_exp <= bias_d1 + ((7 downto 6 => unbiased_exp(5)) & unbiased_exp);

-------------------------- Convert in sign magnitude --------------------------
   not_frac_lzoc <=  frac_lzoc_o xor (24 downto 0 => rippled_carry_d1(31));
   unrounded_frac <= "0" & not_frac_lzoc(23 downto 0) + rippled_carry_d1(31);

---- G and R should be taken from lzoc adding one size more frac lzoc width ----
------------------------------- GRS rounding up -------------------------------
   G <= unrounded_frac(1);
   R <= unrounded_frac(0);
   S <= sticky_lzoc_o;
   round_up <= G and (R or S);
   rounded_frac <= unrounded_frac + round_up;
   post_round_ovf <= rounded_frac(24);

------------------------- post rounding scale handling -------------------------
   post_rounding_exp <= ("0" & biased_exp) + (rounded_frac(24));
   nan_out <= post_rounding_exp(8) or isNaN_d1;
is_zero <= count_lzoc_o(5) when rounded_frac="0000000000000000000000000" else '0';
   final_exp <= post_rounding_exp(7 downto 0) when nan_out = '0' else "11111111";
   arith_o <= (rippled_carry_d1(31) & final_exp(7 downto 0) & rounded_frac(23 downto 1)) when is_zero = '0' else "00000000000000000000000000000000";
end architecture;

--------------------------------------------------------------------------------
--                         DSPBlock_17x24_F200_uid35
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: 
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
-- Input signals: X Y
-- Output signals: R

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library std;
use std.textio.all;
library work;

entity DSPBlock_17x24_F200_uid35 is
    port (clk : in std_logic;
          X : in  std_logic_vector(16 downto 0);
          Y : in  std_logic_vector(23 downto 0);
          R : out  std_logic_vector(40 downto 0)   );
end entity;

architecture arch of DSPBlock_17x24_F200_uid35 is
signal Mint :  std_logic_vector(40 downto 0);
signal M :  std_logic_vector(40 downto 0);
signal Rtmp :  std_logic_vector(40 downto 0);
begin
   Mint <= std_logic_vector(unsigned(X) * unsigned(Y)); -- multiplier
   M <= Mint(40 downto 0);
   Rtmp <= M;
   R <= Rtmp;
end architecture;

--------------------------------------------------------------------------------
--                          DSPBlock_7x24_F200_uid37
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: 
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
-- Input signals: X Y
-- Output signals: R

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library std;
use std.textio.all;
library work;

entity DSPBlock_7x24_F200_uid37 is
    port (clk : in std_logic;
          X : in  std_logic_vector(6 downto 0);
          Y : in  std_logic_vector(23 downto 0);
          R : out  std_logic_vector(30 downto 0)   );
end entity;

architecture arch of DSPBlock_7x24_F200_uid37 is
signal Mint :  std_logic_vector(30 downto 0);
signal M :  std_logic_vector(30 downto 0);
signal Rtmp :  std_logic_vector(30 downto 0);
begin
   Mint <= std_logic_vector(unsigned(X) * unsigned(Y)); -- multiplier
   M <= Mint(30 downto 0);
   Rtmp <= M;
   R <= Rtmp;
end architecture;

--------------------------------------------------------------------------------
--                           IntAdder_32_F200_uid40
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Bogdan Pasca, Florent de Dinechin (2008-2016)
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
-- Input signals: X Y Cin
-- Output signals: R

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity IntAdder_32_F200_uid40 is
    port (clk : in std_logic;
          X : in  std_logic_vector(31 downto 0);
          Y : in  std_logic_vector(31 downto 0);
          Cin : in  std_logic;
          R : out  std_logic_vector(31 downto 0)   );
end entity;

architecture arch of IntAdder_32_F200_uid40 is
signal Rtmp :  std_logic_vector(31 downto 0);
begin
   Rtmp <= X + Y + Cin;
   R <= Rtmp;
end architecture;

--------------------------------------------------------------------------------
--                          IntMultiplier_F200_uid31
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Martin Kumm, Florent de Dinechin, Kinga Illyes, Bogdan Popa, Bogdan Pasca, 2012
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
-- Input signals: X Y
-- Output signals: R

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library std;
use std.textio.all;
library work;

entity IntMultiplier_F200_uid31 is
    port (clk : in std_logic;
          X : in  std_logic_vector(23 downto 0);
          Y : in  std_logic_vector(23 downto 0);
          R : out  std_logic_vector(47 downto 0)   );
end entity;

architecture arch of IntMultiplier_F200_uid31 is
   component DSPBlock_17x24_F200_uid35 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(16 downto 0);
             Y : in  std_logic_vector(23 downto 0);
             R : out  std_logic_vector(40 downto 0)   );
   end component;

   component DSPBlock_7x24_F200_uid37 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(6 downto 0);
             Y : in  std_logic_vector(23 downto 0);
             R : out  std_logic_vector(30 downto 0)   );
   end component;

   component IntAdder_32_F200_uid40 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(31 downto 0);
             Y : in  std_logic_vector(31 downto 0);
             Cin : in  std_logic;
             R : out  std_logic_vector(31 downto 0)   );
   end component;

signal XX_m32 :  std_logic_vector(23 downto 0);
signal YY_m32 :  std_logic_vector(23 downto 0);
signal tile_0_X :  std_logic_vector(16 downto 0);
signal tile_0_Y :  std_logic_vector(23 downto 0);
signal tile_0_output :  std_logic_vector(40 downto 0);
signal tile_0_filtered_output :  std_logic_vector(40 downto 0);
signal bh33_w0_0 :  std_logic;
signal bh33_w1_0 :  std_logic;
signal bh33_w2_0 :  std_logic;
signal bh33_w3_0 :  std_logic;
signal bh33_w4_0 :  std_logic;
signal bh33_w5_0 :  std_logic;
signal bh33_w6_0 :  std_logic;
signal bh33_w7_0 :  std_logic;
signal bh33_w8_0 :  std_logic;
signal bh33_w9_0 :  std_logic;
signal bh33_w10_0 :  std_logic;
signal bh33_w11_0 :  std_logic;
signal bh33_w12_0 :  std_logic;
signal bh33_w13_0 :  std_logic;
signal bh33_w14_0 :  std_logic;
signal bh33_w15_0 :  std_logic;
signal bh33_w16_0 :  std_logic;
signal bh33_w17_0 :  std_logic;
signal bh33_w18_0 :  std_logic;
signal bh33_w19_0 :  std_logic;
signal bh33_w20_0 :  std_logic;
signal bh33_w21_0 :  std_logic;
signal bh33_w22_0 :  std_logic;
signal bh33_w23_0 :  std_logic;
signal bh33_w24_0 :  std_logic;
signal bh33_w25_0 :  std_logic;
signal bh33_w26_0 :  std_logic;
signal bh33_w27_0 :  std_logic;
signal bh33_w28_0 :  std_logic;
signal bh33_w29_0 :  std_logic;
signal bh33_w30_0 :  std_logic;
signal bh33_w31_0 :  std_logic;
signal bh33_w32_0 :  std_logic;
signal bh33_w33_0 :  std_logic;
signal bh33_w34_0 :  std_logic;
signal bh33_w35_0 :  std_logic;
signal bh33_w36_0 :  std_logic;
signal bh33_w37_0 :  std_logic;
signal bh33_w38_0 :  std_logic;
signal bh33_w39_0 :  std_logic;
signal bh33_w40_0 :  std_logic;
signal tile_1_X :  std_logic_vector(6 downto 0);
signal tile_1_Y :  std_logic_vector(23 downto 0);
signal tile_1_output :  std_logic_vector(30 downto 0);
signal tile_1_filtered_output :  std_logic_vector(30 downto 0);
signal bh33_w17_1 :  std_logic;
signal bh33_w18_1 :  std_logic;
signal bh33_w19_1 :  std_logic;
signal bh33_w20_1 :  std_logic;
signal bh33_w21_1 :  std_logic;
signal bh33_w22_1 :  std_logic;
signal bh33_w23_1 :  std_logic;
signal bh33_w24_1 :  std_logic;
signal bh33_w25_1 :  std_logic;
signal bh33_w26_1 :  std_logic;
signal bh33_w27_1 :  std_logic;
signal bh33_w28_1 :  std_logic;
signal bh33_w29_1 :  std_logic;
signal bh33_w30_1 :  std_logic;
signal bh33_w31_1 :  std_logic;
signal bh33_w32_1 :  std_logic;
signal bh33_w33_1 :  std_logic;
signal bh33_w34_1 :  std_logic;
signal bh33_w35_1 :  std_logic;
signal bh33_w36_1 :  std_logic;
signal bh33_w37_1 :  std_logic;
signal bh33_w38_1 :  std_logic;
signal bh33_w39_1 :  std_logic;
signal bh33_w40_1 :  std_logic;
signal bh33_w41_0 :  std_logic;
signal bh33_w42_0 :  std_logic;
signal bh33_w43_0 :  std_logic;
signal bh33_w44_0 :  std_logic;
signal bh33_w45_0 :  std_logic;
signal bh33_w46_0 :  std_logic;
signal bh33_w47_0 :  std_logic;
signal tmp_bitheapResult_bh33_16 :  std_logic_vector(16 downto 0);
signal bitheapFinalAdd_bh33_In0 :  std_logic_vector(31 downto 0);
signal bitheapFinalAdd_bh33_In1 :  std_logic_vector(31 downto 0);
signal bitheapFinalAdd_bh33_Cin :  std_logic;
signal bitheapFinalAdd_bh33_Out :  std_logic_vector(31 downto 0);
signal bitheapResult_bh33 :  std_logic_vector(47 downto 0);
begin
   XX_m32 <= X ;
   YY_m32 <= Y ;
   tile_0_X <= X(16 downto 0);
   tile_0_Y <= Y(23 downto 0);
   tile_0_mult: DSPBlock_17x24_F200_uid35
      port map ( clk  => clk,
                 X => tile_0_X,
                 Y => tile_0_Y,
                 R => tile_0_output);

tile_0_filtered_output <= tile_0_output(40 downto 0);
   bh33_w0_0 <= tile_0_filtered_output(0);
   bh33_w1_0 <= tile_0_filtered_output(1);
   bh33_w2_0 <= tile_0_filtered_output(2);
   bh33_w3_0 <= tile_0_filtered_output(3);
   bh33_w4_0 <= tile_0_filtered_output(4);
   bh33_w5_0 <= tile_0_filtered_output(5);
   bh33_w6_0 <= tile_0_filtered_output(6);
   bh33_w7_0 <= tile_0_filtered_output(7);
   bh33_w8_0 <= tile_0_filtered_output(8);
   bh33_w9_0 <= tile_0_filtered_output(9);
   bh33_w10_0 <= tile_0_filtered_output(10);
   bh33_w11_0 <= tile_0_filtered_output(11);
   bh33_w12_0 <= tile_0_filtered_output(12);
   bh33_w13_0 <= tile_0_filtered_output(13);
   bh33_w14_0 <= tile_0_filtered_output(14);
   bh33_w15_0 <= tile_0_filtered_output(15);
   bh33_w16_0 <= tile_0_filtered_output(16);
   bh33_w17_0 <= tile_0_filtered_output(17);
   bh33_w18_0 <= tile_0_filtered_output(18);
   bh33_w19_0 <= tile_0_filtered_output(19);
   bh33_w20_0 <= tile_0_filtered_output(20);
   bh33_w21_0 <= tile_0_filtered_output(21);
   bh33_w22_0 <= tile_0_filtered_output(22);
   bh33_w23_0 <= tile_0_filtered_output(23);
   bh33_w24_0 <= tile_0_filtered_output(24);
   bh33_w25_0 <= tile_0_filtered_output(25);
   bh33_w26_0 <= tile_0_filtered_output(26);
   bh33_w27_0 <= tile_0_filtered_output(27);
   bh33_w28_0 <= tile_0_filtered_output(28);
   bh33_w29_0 <= tile_0_filtered_output(29);
   bh33_w30_0 <= tile_0_filtered_output(30);
   bh33_w31_0 <= tile_0_filtered_output(31);
   bh33_w32_0 <= tile_0_filtered_output(32);
   bh33_w33_0 <= tile_0_filtered_output(33);
   bh33_w34_0 <= tile_0_filtered_output(34);
   bh33_w35_0 <= tile_0_filtered_output(35);
   bh33_w36_0 <= tile_0_filtered_output(36);
   bh33_w37_0 <= tile_0_filtered_output(37);
   bh33_w38_0 <= tile_0_filtered_output(38);
   bh33_w39_0 <= tile_0_filtered_output(39);
   bh33_w40_0 <= tile_0_filtered_output(40);
   tile_1_X <= X(23 downto 17);
   tile_1_Y <= Y(23 downto 0);
   tile_1_mult: DSPBlock_7x24_F200_uid37
      port map ( clk  => clk,
                 X => tile_1_X,
                 Y => tile_1_Y,
                 R => tile_1_output);

tile_1_filtered_output <= tile_1_output(30 downto 0);
   bh33_w17_1 <= tile_1_filtered_output(0);
   bh33_w18_1 <= tile_1_filtered_output(1);
   bh33_w19_1 <= tile_1_filtered_output(2);
   bh33_w20_1 <= tile_1_filtered_output(3);
   bh33_w21_1 <= tile_1_filtered_output(4);
   bh33_w22_1 <= tile_1_filtered_output(5);
   bh33_w23_1 <= tile_1_filtered_output(6);
   bh33_w24_1 <= tile_1_filtered_output(7);
   bh33_w25_1 <= tile_1_filtered_output(8);
   bh33_w26_1 <= tile_1_filtered_output(9);
   bh33_w27_1 <= tile_1_filtered_output(10);
   bh33_w28_1 <= tile_1_filtered_output(11);
   bh33_w29_1 <= tile_1_filtered_output(12);
   bh33_w30_1 <= tile_1_filtered_output(13);
   bh33_w31_1 <= tile_1_filtered_output(14);
   bh33_w32_1 <= tile_1_filtered_output(15);
   bh33_w33_1 <= tile_1_filtered_output(16);
   bh33_w34_1 <= tile_1_filtered_output(17);
   bh33_w35_1 <= tile_1_filtered_output(18);
   bh33_w36_1 <= tile_1_filtered_output(19);
   bh33_w37_1 <= tile_1_filtered_output(20);
   bh33_w38_1 <= tile_1_filtered_output(21);
   bh33_w39_1 <= tile_1_filtered_output(22);
   bh33_w40_1 <= tile_1_filtered_output(23);
   bh33_w41_0 <= tile_1_filtered_output(24);
   bh33_w42_0 <= tile_1_filtered_output(25);
   bh33_w43_0 <= tile_1_filtered_output(26);
   bh33_w44_0 <= tile_1_filtered_output(27);
   bh33_w45_0 <= tile_1_filtered_output(28);
   bh33_w46_0 <= tile_1_filtered_output(29);
   bh33_w47_0 <= tile_1_filtered_output(30);

   -- Adding the constant bits
      -- All the constant bits are zero, nothing to add

   tmp_bitheapResult_bh33_16 <= bh33_w16_0 & bh33_w15_0 & bh33_w14_0 & bh33_w13_0 & bh33_w12_0 & bh33_w11_0 & bh33_w10_0 & bh33_w9_0 & bh33_w8_0 & bh33_w7_0 & bh33_w6_0 & bh33_w5_0 & bh33_w4_0 & bh33_w3_0 & bh33_w2_0 & bh33_w1_0 & bh33_w0_0;

   bitheapFinalAdd_bh33_In0 <= "0" & bh33_w47_0 & bh33_w46_0 & bh33_w45_0 & bh33_w44_0 & bh33_w43_0 & bh33_w42_0 & bh33_w41_0 & bh33_w40_0 & bh33_w39_0 & bh33_w38_0 & bh33_w37_0 & bh33_w36_0 & bh33_w35_0 & bh33_w34_0 & bh33_w33_0 & bh33_w32_0 & bh33_w31_0 & bh33_w30_0 & bh33_w29_0 & bh33_w28_0 & bh33_w27_0 & bh33_w26_0 & bh33_w25_0 & bh33_w24_0 & bh33_w23_0 & bh33_w22_0 & bh33_w21_0 & bh33_w20_0 & bh33_w19_0 & bh33_w18_0 & bh33_w17_0;
   bitheapFinalAdd_bh33_In1 <= "0" & "0" & "0" & "0" & "0" & "0" & "0" & "0" & bh33_w40_1 & bh33_w39_1 & bh33_w38_1 & bh33_w37_1 & bh33_w36_1 & bh33_w35_1 & bh33_w34_1 & bh33_w33_1 & bh33_w32_1 & bh33_w31_1 & bh33_w30_1 & bh33_w29_1 & bh33_w28_1 & bh33_w27_1 & bh33_w26_1 & bh33_w25_1 & bh33_w24_1 & bh33_w23_1 & bh33_w22_1 & bh33_w21_1 & bh33_w20_1 & bh33_w19_1 & bh33_w18_1 & bh33_w17_1;
   bitheapFinalAdd_bh33_Cin <= '0';

   bitheapFinalAdd_bh33: IntAdder_32_F200_uid40
      port map ( clk  => clk,
                 Cin => bitheapFinalAdd_bh33_Cin,
                 X => bitheapFinalAdd_bh33_In0,
                 Y => bitheapFinalAdd_bh33_In1,
                 R => bitheapFinalAdd_bh33_Out);
   bitheapResult_bh33 <= bitheapFinalAdd_bh33_Out(30 downto 0) & tmp_bitheapResult_bh33_16;
   R <= bitheapResult_bh33(47 downto 0);
end architecture;

--------------------------------------------------------------------------------
--                    LeftShifter48_by_max_511_F200_uid42
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Bogdan Pasca (2008-2011), Florent de Dinechin (2008-2019)
--------------------------------------------------------------------------------
-- Pipeline depth: 2 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
-- Input signals: X S padBit
-- Output signals: R

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
library std;
use std.textio.all;
library work;

entity LeftShifter48_by_max_511_F200_uid42 is
    port (clk : in std_logic;
          X : in  std_logic_vector(47 downto 0);
          S : in  std_logic_vector(8 downto 0);
          padBit : in  std_logic;
          R : out  std_logic_vector(558 downto 0)   );
end entity;

architecture arch of LeftShifter48_by_max_511_F200_uid42 is
signal ps, ps_d1, ps_d2 :  std_logic_vector(8 downto 0);
signal level0 :  std_logic_vector(47 downto 0);
signal level1 :  std_logic_vector(48 downto 0);
signal level2 :  std_logic_vector(50 downto 0);
signal level3 :  std_logic_vector(54 downto 0);
signal level4 :  std_logic_vector(62 downto 0);
signal level5, level5_d1 :  std_logic_vector(78 downto 0);
signal level6 :  std_logic_vector(110 downto 0);
signal level7, level7_d1 :  std_logic_vector(174 downto 0);
signal level8 :  std_logic_vector(302 downto 0);
signal level9 :  std_logic_vector(558 downto 0);
signal padBit_d1, padBit_d2 :  std_logic;
begin
   process(clk)
      begin
         if clk'event and clk = '1' then
            ps_d1 <=  ps;
            ps_d2 <=  ps_d1;
            level5_d1 <=  level5;
            level7_d1 <=  level7;
            padBit_d1 <=  padBit;
            padBit_d2 <=  padBit_d1;
         end if;
      end process;
   ps<= S;
   level0<= X;
   level1<= level0 & (0 downto 0 => '0') when ps(0)= '1' else     (0 downto 0 => padBit) & level0;
   level2<= level1 & (1 downto 0 => '0') when ps(1)= '1' else     (1 downto 0 => padBit) & level1;
   level3<= level2 & (3 downto 0 => '0') when ps(2)= '1' else     (3 downto 0 => padBit) & level2;
   level4<= level3 & (7 downto 0 => '0') when ps(3)= '1' else     (7 downto 0 => padBit) & level3;
   level5<= level4 & (15 downto 0 => '0') when ps(4)= '1' else     (15 downto 0 => padBit) & level4;
   level6<= level5_d1 & (31 downto 0 => '0') when ps_d1(5)= '1' else     (31 downto 0 => padBit_d1) & level5_d1;
   level7<= level6 & (63 downto 0 => '0') when ps_d1(6)= '1' else     (63 downto 0 => padBit_d1) & level6;
   level8<= level7_d1 & (127 downto 0 => '0') when ps_d2(7)= '1' else     (127 downto 0 => padBit_d2) & level7_d1;
   level9<= level8 & (255 downto 0 => '0') when ps_d2(8)= '1' else     (255 downto 0 => padBit_d2) & level8;
   R <= level9(558 downto 0);
end architecture;

--------------------------------------------------------------------------------
--                                   s3fdp
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 3 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
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
          S3_x : in  std_logic_vector(33 downto 0);
          S3_y : in  std_logic_vector(33 downto 0);
          FTZ : in  std_logic;
          EOB : in  std_logic;
          A : out  std_logic_vector(31 downto 0);
          EOB_Q : out  std_logic;
          isNaN : out  std_logic   );
end entity;

architecture arch of s3fdp is
   component IntMultiplier_F200_uid31 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(23 downto 0);
             Y : in  std_logic_vector(23 downto 0);
             R : out  std_logic_vector(47 downto 0)   );
   end component;

   component LeftShifter48_by_max_511_F200_uid42 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(47 downto 0);
             S : in  std_logic_vector(8 downto 0);
             padBit : in  std_logic;
             R : out  std_logic_vector(558 downto 0)   );
   end component;

signal sign_X :  std_logic;
signal sign_Y :  std_logic;
signal sign_M, sign_M_d1, sign_M_d2 :  std_logic;
signal isNaN_X :  std_logic;
signal isNaN_Y :  std_logic;
signal isNaN_M, isNaN_M_d1, isNaN_M_d2 :  std_logic;
signal significand_X :  std_logic_vector(23 downto 0);
signal significand_Y :  std_logic_vector(23 downto 0);
signal significand_product :  std_logic_vector(47 downto 0);
signal scale_X_biased :  std_logic_vector(7 downto 0);
signal scale_Y_biased :  std_logic_vector(7 downto 0);
signal scale_product_twice_biased :  std_logic_vector(8 downto 0);
signal significand_product_cpt1 :  std_logic_vector(47 downto 0);
signal shift_value :  std_logic_vector(8 downto 0);
signal shifted_significand :  std_logic_vector(558 downto 0);
signal too_small, too_small_d1, too_small_d2 :  std_logic;
signal too_big, too_big_d1, too_big_d2 :  std_logic;
signal ext_summand1c :  std_logic_vector(31 downto 0);
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
signal summand_0 :  std_logic_vector(31 downto 0);
signal summand_and_carry_0 :  std_logic_vector(32 downto 0);
signal acc_0, acc_0_d1 :  std_logic_vector(32 downto 0);
signal acc_0_q :  std_logic_vector(32 downto 0);
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
   sign_X <= S3_x(32);
   sign_Y <= S3_y(32);
   sign_M <= sign_X xor sign_Y;

---------------------------- NaN product processing ----------------------------
   isNaN_X <= S3_x(33);
   isNaN_Y <= S3_y(33);
   isNaN_M <= isNaN_X or isNaN_Y;

---------------------------- significand processing ----------------------------
   significand_X <= S3_x(31 downto 8);
   significand_Y <= S3_y(31 downto 8);
   significand_product_inst: IntMultiplier_F200_uid31
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
   shift_value <= (scale_product_twice_biased) - (238);
   significand_product_shifter_inst: LeftShifter48_by_max_511_F200_uid42
      port map ( clk  => clk,
                 S => shift_value,
                 X => significand_product_cpt1,
                 padBit => sign_M,
                 R => shifted_significand);

-------------- detect too low scale for this specific scratchpad --------------
   too_small <= '1' when (shift_value(8)='1') else '0';

-------------- detect too big scale for this specific scratchpad --------------
   too_big <= '1' when (unsigned(shift_value) > 22 and too_small='0') else '0';

--------------- shifted significand part select to form summand ---------------
   ext_summand1c <= "00000000000000000000000000000000" when too_small_d2='1' else shifted_significand(78 downto 47);
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
   summand_0 <= ext_summand1c(31 downto 0);
   summand_and_carry_0 <= ("0" & summand_0) + carry_0;
   acc_0 <= (("0" & acc_0_q(31 downto 0)) + summand_and_carry_0) when (not_ftz_sync='1') else
            summand_and_carry_0;

-------------------------------- Output Compose --------------------------------
   A <= acc_0_q(31 downto 0);

   EOB_Q <= EOB_internal_delayed;
   isNaN <= isNaN_delayed;
end architecture;

--------------------------------------------------------------------------------
--                                   PE_S3
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
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
          s3_row_i_A : in  std_logic_vector(33 downto 0);
          s3_col_j_B : in  std_logic_vector(33 downto 0);
          C_out : in  std_logic_vector(32 downto 0);
          SOB : in  std_logic;
          EOB : in  std_logic;
          s3_row_im1_A : out  std_logic_vector(33 downto 0);
          s3_col_jm1_B : out  std_logic_vector(33 downto 0);
          SOB_Q : out  std_logic;
          EOB_Q : out  std_logic;
          C_out_Q : out  std_logic_vector(32 downto 0)   );
end entity;

architecture arch of PE_S3 is
   component s3fdp is
      port ( clk, rst : in std_logic;
             S3_x : in  std_logic_vector(33 downto 0);
             S3_y : in  std_logic_vector(33 downto 0);
             FTZ : in  std_logic;
             EOB : in  std_logic;
             A : out  std_logic_vector(31 downto 0);
             EOB_Q : out  std_logic;
             isNaN : out  std_logic   );
   end component;

signal s3_row_i_A_q :  std_logic_vector(33 downto 0);
signal s3_col_j_B_q :  std_logic_vector(33 downto 0);
signal sob_delayed :  std_logic;
signal eob_delayed :  std_logic;
signal mux_C_out, mux_C_out_d1, mux_C_out_d2 :  std_logic_vector(32 downto 0);
signal mux_C_out_HSSD :  std_logic_vector(32 downto 0);
signal isNaN_s3fdp :  std_logic;
signal EOB_s3fdp :  std_logic;
signal A_s3fdp :  std_logic_vector(31 downto 0);
signal s3_row_i_A_d1 :  std_logic_vector(33 downto 0);
signal s3_col_j_B_d1 :  std_logic_vector(33 downto 0);
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
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
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
          rowsA : in  std_logic_vector(543 downto 0);
          colsB : in  std_logic_vector(509 downto 0);
          SOB : in  std_logic;
          EOB : in  std_logic;
          colsC : out  std_logic_vector(494 downto 0);
          EOB_Q_o : out  std_logic   );
end entity;

architecture arch of SystolicArrayKernel is
   component PE_S3 is
      port ( clk, rst : in std_logic;
             s3_row_i_A : in  std_logic_vector(33 downto 0);
             s3_col_j_B : in  std_logic_vector(33 downto 0);
             C_out : in  std_logic_vector(32 downto 0);
             SOB : in  std_logic;
             EOB : in  std_logic;
             s3_row_im1_A : out  std_logic_vector(33 downto 0);
             s3_col_jm1_B : out  std_logic_vector(33 downto 0);
             SOB_Q : out  std_logic;
             EOB_Q : out  std_logic;
             C_out_Q : out  std_logic_vector(32 downto 0)   );
   end component;

type T_2D_LAICPT2_np1_m is array(16 downto 0, 14 downto 0) of std_logic_vector(32 downto 0);
type T_2D_n_mp1 is array(15 downto 0, 15 downto 0) of std_logic_vector(33 downto 0);
type T_2D_np1_m is array(16 downto 0, 14 downto 0) of std_logic_vector(33 downto 0);
type T_2D_np1_m_logic is array(16 downto 0, 14 downto 0) of std_logic;
signal systolic_wires_rows_2D : T_2D_n_mp1;
signal systolic_wires_cols_2D : T_2D_np1_m;
signal systolic_sob_2D : T_2D_np1_m_logic;
signal systolic_eob_2D : T_2D_np1_m_logic;
signal systolic_C_out_2D : T_2D_LAICPT2_np1_m;
begin

----------------- Connect bus of B columns to top edges SA PEs -----------------
   cols_in: for JJ in 0 to 14 generate
      systolic_wires_cols_2D(0,JJ) <= colsB(((JJ+1)*34)-1 downto (JJ*34));
   end generate;

------------------ Connect bus of A rows to left edges SA PEs ------------------
   rows_in: for II in 0 to 15 generate
      systolic_wires_rows_2D(II,0) <= rowsA(((II+1)*34)-1 downto (II*34));
   end generate;

-------------- Connect the Start of Block signals of the TOP PEs --------------
   systolic_sob_2D(0,0) <= SOB;
   sob_1st_row: for JJ in 1 to 14 generate
      systolic_sob_2D(0,JJ) <= systolic_sob_2D(1,JJ-1);
   end generate;

--------------- Connect the End of Block signals of the TOP PEs ---------------
   systolic_eob_2D(0,0) <= EOB;
   eob_1st_row: for JJ in 1 to 14 generate
      systolic_eob_2D(0,JJ) <= systolic_eob_2D(1,JJ-1);
   end generate;

----------- Connect with 0s the input C carry out scheme of TOP PEs -----------
   C_out_input_1st_row: for JJ in 0 to 14 generate
      systolic_C_out_2D(0,JJ) <= "000000000000000000000000000000000";
   end generate;

------------------------- Connect PEs locally together -------------------------
   rows: for II in 0 to 15 generate
      cols: for JJ in 0 to 14 generate
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
   cols_C_out: for JJ in 0 to 14 generate
      colsC(((JJ+1)*33)-1 downto (JJ*33)) <= systolic_C_out_2D(16,JJ);
   end generate;

------ Connect PE(N-1,M-1) EOB_Q to out world for valid data computation ------
   EOB_Q_o <= systolic_eob_2D(16,14);

end architecture;

--------------------------------------------------------------------------------
--                               SystolicArray
--              (SA_orthogonal_16w15h_ieee_8_23_HSSD_F200_uid2)
-- VHDL generated for VirtexUltrascalePlus @ 200MHz
-- This operator is part of the Infinite Virtual Library FloPoCoLib
-- All rights reserved 
-- Authors: Ledoux Louis - BSC / UPC
--------------------------------------------------------------------------------
-- Pipeline depth: 0 cycles
-- Clock period (ns): 5
-- Target frequency (MHz): 200
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
          rowsA : in  std_logic_vector(511 downto 0);
          colsB : in  std_logic_vector(479 downto 0);
          SOB : in  std_logic;
          EOB : in  std_logic;
          colsC : out  std_logic_vector(479 downto 0);
          EOB_Q_o : out  std_logic   );
end entity;

architecture arch of SystolicArray is
   component Arith_to_S3 is
      port ( clk : in std_logic;
             arith_i : in  std_logic_vector(31 downto 0);
             S3_o : out  std_logic_vector(33 downto 0)   );
   end component;

   component l2a is
      port ( clk : in std_logic;
             A : in  std_logic_vector(31 downto 0);
             isNaN : in  std_logic;
             arith_o : out  std_logic_vector(31 downto 0)   );
   end component;

   component SystolicArrayKernel is
      port ( clk, rst : in std_logic;
             rowsA : in  std_logic_vector(543 downto 0);
             colsB : in  std_logic_vector(509 downto 0);
             SOB : in  std_logic;
             EOB : in  std_logic;
             colsC : out  std_logic_vector(494 downto 0);
             EOB_Q_o : out  std_logic   );
   end component;

type array_M_dense is array(14 downto 0) of std_logic_vector(31 downto 0);
type array_M_s3 is array(14 downto 0) of std_logic_vector(33 downto 0);
type array_N_dense is array(15 downto 0) of std_logic_vector(31 downto 0);
type array_N_s3 is array(15 downto 0) of std_logic_vector(33 downto 0);
signal arith_in_row_0 :  std_logic_vector(31 downto 0);
signal arith_in_row_0_q0 :  std_logic_vector(31 downto 0);
signal arith_in_row_1, arith_in_row_1_d1 :  std_logic_vector(31 downto 0);
signal arith_in_row_1_q1 :  std_logic_vector(31 downto 0);
signal arith_in_row_2, arith_in_row_2_d1, arith_in_row_2_d2 :  std_logic_vector(31 downto 0);
signal arith_in_row_2_q2 :  std_logic_vector(31 downto 0);
signal arith_in_row_3, arith_in_row_3_d1, arith_in_row_3_d2, arith_in_row_3_d3 :  std_logic_vector(31 downto 0);
signal arith_in_row_3_q3 :  std_logic_vector(31 downto 0);
signal arith_in_row_4, arith_in_row_4_d1, arith_in_row_4_d2, arith_in_row_4_d3, arith_in_row_4_d4 :  std_logic_vector(31 downto 0);
signal arith_in_row_4_q4 :  std_logic_vector(31 downto 0);
signal arith_in_row_5, arith_in_row_5_d1, arith_in_row_5_d2, arith_in_row_5_d3, arith_in_row_5_d4, arith_in_row_5_d5 :  std_logic_vector(31 downto 0);
signal arith_in_row_5_q5 :  std_logic_vector(31 downto 0);
signal arith_in_row_6, arith_in_row_6_d1, arith_in_row_6_d2, arith_in_row_6_d3, arith_in_row_6_d4, arith_in_row_6_d5, arith_in_row_6_d6 :  std_logic_vector(31 downto 0);
signal arith_in_row_6_q6 :  std_logic_vector(31 downto 0);
signal arith_in_row_7, arith_in_row_7_d1, arith_in_row_7_d2, arith_in_row_7_d3, arith_in_row_7_d4, arith_in_row_7_d5, arith_in_row_7_d6, arith_in_row_7_d7 :  std_logic_vector(31 downto 0);
signal arith_in_row_7_q7 :  std_logic_vector(31 downto 0);
signal arith_in_row_8, arith_in_row_8_d1, arith_in_row_8_d2, arith_in_row_8_d3, arith_in_row_8_d4, arith_in_row_8_d5, arith_in_row_8_d6, arith_in_row_8_d7, arith_in_row_8_d8 :  std_logic_vector(31 downto 0);
signal arith_in_row_8_q8 :  std_logic_vector(31 downto 0);
signal arith_in_row_9, arith_in_row_9_d1, arith_in_row_9_d2, arith_in_row_9_d3, arith_in_row_9_d4, arith_in_row_9_d5, arith_in_row_9_d6, arith_in_row_9_d7, arith_in_row_9_d8, arith_in_row_9_d9 :  std_logic_vector(31 downto 0);
signal arith_in_row_9_q9 :  std_logic_vector(31 downto 0);
signal arith_in_row_10, arith_in_row_10_d1, arith_in_row_10_d2, arith_in_row_10_d3, arith_in_row_10_d4, arith_in_row_10_d5, arith_in_row_10_d6, arith_in_row_10_d7, arith_in_row_10_d8, arith_in_row_10_d9, arith_in_row_10_d10 :  std_logic_vector(31 downto 0);
signal arith_in_row_10_q10 :  std_logic_vector(31 downto 0);
signal arith_in_row_11, arith_in_row_11_d1, arith_in_row_11_d2, arith_in_row_11_d3, arith_in_row_11_d4, arith_in_row_11_d5, arith_in_row_11_d6, arith_in_row_11_d7, arith_in_row_11_d8, arith_in_row_11_d9, arith_in_row_11_d10, arith_in_row_11_d11 :  std_logic_vector(31 downto 0);
signal arith_in_row_11_q11 :  std_logic_vector(31 downto 0);
signal arith_in_row_12, arith_in_row_12_d1, arith_in_row_12_d2, arith_in_row_12_d3, arith_in_row_12_d4, arith_in_row_12_d5, arith_in_row_12_d6, arith_in_row_12_d7, arith_in_row_12_d8, arith_in_row_12_d9, arith_in_row_12_d10, arith_in_row_12_d11, arith_in_row_12_d12 :  std_logic_vector(31 downto 0);
signal arith_in_row_12_q12 :  std_logic_vector(31 downto 0);
signal arith_in_row_13, arith_in_row_13_d1, arith_in_row_13_d2, arith_in_row_13_d3, arith_in_row_13_d4, arith_in_row_13_d5, arith_in_row_13_d6, arith_in_row_13_d7, arith_in_row_13_d8, arith_in_row_13_d9, arith_in_row_13_d10, arith_in_row_13_d11, arith_in_row_13_d12, arith_in_row_13_d13 :  std_logic_vector(31 downto 0);
signal arith_in_row_13_q13 :  std_logic_vector(31 downto 0);
signal arith_in_row_14, arith_in_row_14_d1, arith_in_row_14_d2, arith_in_row_14_d3, arith_in_row_14_d4, arith_in_row_14_d5, arith_in_row_14_d6, arith_in_row_14_d7, arith_in_row_14_d8, arith_in_row_14_d9, arith_in_row_14_d10, arith_in_row_14_d11, arith_in_row_14_d12, arith_in_row_14_d13, arith_in_row_14_d14 :  std_logic_vector(31 downto 0);
signal arith_in_row_14_q14 :  std_logic_vector(31 downto 0);
signal arith_in_row_15, arith_in_row_15_d1, arith_in_row_15_d2, arith_in_row_15_d3, arith_in_row_15_d4, arith_in_row_15_d5, arith_in_row_15_d6, arith_in_row_15_d7, arith_in_row_15_d8, arith_in_row_15_d9, arith_in_row_15_d10, arith_in_row_15_d11, arith_in_row_15_d12, arith_in_row_15_d13, arith_in_row_15_d14, arith_in_row_15_d15 :  std_logic_vector(31 downto 0);
signal arith_in_row_15_q15 :  std_logic_vector(31 downto 0);
signal arith_in_col_0 :  std_logic_vector(31 downto 0);
signal arith_in_col_0_q0 :  std_logic_vector(31 downto 0);
signal arith_in_col_1, arith_in_col_1_d1 :  std_logic_vector(31 downto 0);
signal arith_in_col_1_q1 :  std_logic_vector(31 downto 0);
signal arith_in_col_2, arith_in_col_2_d1, arith_in_col_2_d2 :  std_logic_vector(31 downto 0);
signal arith_in_col_2_q2 :  std_logic_vector(31 downto 0);
signal arith_in_col_3, arith_in_col_3_d1, arith_in_col_3_d2, arith_in_col_3_d3 :  std_logic_vector(31 downto 0);
signal arith_in_col_3_q3 :  std_logic_vector(31 downto 0);
signal arith_in_col_4, arith_in_col_4_d1, arith_in_col_4_d2, arith_in_col_4_d3, arith_in_col_4_d4 :  std_logic_vector(31 downto 0);
signal arith_in_col_4_q4 :  std_logic_vector(31 downto 0);
signal arith_in_col_5, arith_in_col_5_d1, arith_in_col_5_d2, arith_in_col_5_d3, arith_in_col_5_d4, arith_in_col_5_d5 :  std_logic_vector(31 downto 0);
signal arith_in_col_5_q5 :  std_logic_vector(31 downto 0);
signal arith_in_col_6, arith_in_col_6_d1, arith_in_col_6_d2, arith_in_col_6_d3, arith_in_col_6_d4, arith_in_col_6_d5, arith_in_col_6_d6 :  std_logic_vector(31 downto 0);
signal arith_in_col_6_q6 :  std_logic_vector(31 downto 0);
signal arith_in_col_7, arith_in_col_7_d1, arith_in_col_7_d2, arith_in_col_7_d3, arith_in_col_7_d4, arith_in_col_7_d5, arith_in_col_7_d6, arith_in_col_7_d7 :  std_logic_vector(31 downto 0);
signal arith_in_col_7_q7 :  std_logic_vector(31 downto 0);
signal arith_in_col_8, arith_in_col_8_d1, arith_in_col_8_d2, arith_in_col_8_d3, arith_in_col_8_d4, arith_in_col_8_d5, arith_in_col_8_d6, arith_in_col_8_d7, arith_in_col_8_d8 :  std_logic_vector(31 downto 0);
signal arith_in_col_8_q8 :  std_logic_vector(31 downto 0);
signal arith_in_col_9, arith_in_col_9_d1, arith_in_col_9_d2, arith_in_col_9_d3, arith_in_col_9_d4, arith_in_col_9_d5, arith_in_col_9_d6, arith_in_col_9_d7, arith_in_col_9_d8, arith_in_col_9_d9 :  std_logic_vector(31 downto 0);
signal arith_in_col_9_q9 :  std_logic_vector(31 downto 0);
signal arith_in_col_10, arith_in_col_10_d1, arith_in_col_10_d2, arith_in_col_10_d3, arith_in_col_10_d4, arith_in_col_10_d5, arith_in_col_10_d6, arith_in_col_10_d7, arith_in_col_10_d8, arith_in_col_10_d9, arith_in_col_10_d10 :  std_logic_vector(31 downto 0);
signal arith_in_col_10_q10 :  std_logic_vector(31 downto 0);
signal arith_in_col_11, arith_in_col_11_d1, arith_in_col_11_d2, arith_in_col_11_d3, arith_in_col_11_d4, arith_in_col_11_d5, arith_in_col_11_d6, arith_in_col_11_d7, arith_in_col_11_d8, arith_in_col_11_d9, arith_in_col_11_d10, arith_in_col_11_d11 :  std_logic_vector(31 downto 0);
signal arith_in_col_11_q11 :  std_logic_vector(31 downto 0);
signal arith_in_col_12, arith_in_col_12_d1, arith_in_col_12_d2, arith_in_col_12_d3, arith_in_col_12_d4, arith_in_col_12_d5, arith_in_col_12_d6, arith_in_col_12_d7, arith_in_col_12_d8, arith_in_col_12_d9, arith_in_col_12_d10, arith_in_col_12_d11, arith_in_col_12_d12 :  std_logic_vector(31 downto 0);
signal arith_in_col_12_q12 :  std_logic_vector(31 downto 0);
signal arith_in_col_13, arith_in_col_13_d1, arith_in_col_13_d2, arith_in_col_13_d3, arith_in_col_13_d4, arith_in_col_13_d5, arith_in_col_13_d6, arith_in_col_13_d7, arith_in_col_13_d8, arith_in_col_13_d9, arith_in_col_13_d10, arith_in_col_13_d11, arith_in_col_13_d12, arith_in_col_13_d13 :  std_logic_vector(31 downto 0);
signal arith_in_col_13_q13 :  std_logic_vector(31 downto 0);
signal arith_in_col_14, arith_in_col_14_d1, arith_in_col_14_d2, arith_in_col_14_d3, arith_in_col_14_d4, arith_in_col_14_d5, arith_in_col_14_d6, arith_in_col_14_d7, arith_in_col_14_d8, arith_in_col_14_d9, arith_in_col_14_d10, arith_in_col_14_d11, arith_in_col_14_d12, arith_in_col_14_d13, arith_in_col_14_d14 :  std_logic_vector(31 downto 0);
signal arith_in_col_14_q14 :  std_logic_vector(31 downto 0);
signal colsC_LAICPT2 :  std_logic_vector(494 downto 0);
signal SOB_select :  std_logic;
signal SOB_q0 :  std_logic;
signal EOB_select :  std_logic;
signal EOB_q0 :  std_logic;
signal LAICPT2_to_arith :  std_logic_vector(479 downto 0);
signal arith_out_col_out_0, arith_out_col_out_0_d1, arith_out_col_out_0_d2, arith_out_col_out_0_d3, arith_out_col_out_0_d4, arith_out_col_out_0_d5, arith_out_col_out_0_d6, arith_out_col_out_0_d7, arith_out_col_out_0_d8, arith_out_col_out_0_d9, arith_out_col_out_0_d10, arith_out_col_out_0_d11, arith_out_col_out_0_d12, arith_out_col_out_0_d13, arith_out_col_out_0_d14 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_0_q14 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_1, arith_out_col_out_1_d1, arith_out_col_out_1_d2, arith_out_col_out_1_d3, arith_out_col_out_1_d4, arith_out_col_out_1_d5, arith_out_col_out_1_d6, arith_out_col_out_1_d7, arith_out_col_out_1_d8, arith_out_col_out_1_d9, arith_out_col_out_1_d10, arith_out_col_out_1_d11, arith_out_col_out_1_d12, arith_out_col_out_1_d13 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_1_q13 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_2, arith_out_col_out_2_d1, arith_out_col_out_2_d2, arith_out_col_out_2_d3, arith_out_col_out_2_d4, arith_out_col_out_2_d5, arith_out_col_out_2_d6, arith_out_col_out_2_d7, arith_out_col_out_2_d8, arith_out_col_out_2_d9, arith_out_col_out_2_d10, arith_out_col_out_2_d11, arith_out_col_out_2_d12 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_2_q12 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_3, arith_out_col_out_3_d1, arith_out_col_out_3_d2, arith_out_col_out_3_d3, arith_out_col_out_3_d4, arith_out_col_out_3_d5, arith_out_col_out_3_d6, arith_out_col_out_3_d7, arith_out_col_out_3_d8, arith_out_col_out_3_d9, arith_out_col_out_3_d10, arith_out_col_out_3_d11 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_3_q11 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_4, arith_out_col_out_4_d1, arith_out_col_out_4_d2, arith_out_col_out_4_d3, arith_out_col_out_4_d4, arith_out_col_out_4_d5, arith_out_col_out_4_d6, arith_out_col_out_4_d7, arith_out_col_out_4_d8, arith_out_col_out_4_d9, arith_out_col_out_4_d10 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_4_q10 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_5, arith_out_col_out_5_d1, arith_out_col_out_5_d2, arith_out_col_out_5_d3, arith_out_col_out_5_d4, arith_out_col_out_5_d5, arith_out_col_out_5_d6, arith_out_col_out_5_d7, arith_out_col_out_5_d8, arith_out_col_out_5_d9 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_5_q9 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_6, arith_out_col_out_6_d1, arith_out_col_out_6_d2, arith_out_col_out_6_d3, arith_out_col_out_6_d4, arith_out_col_out_6_d5, arith_out_col_out_6_d6, arith_out_col_out_6_d7, arith_out_col_out_6_d8 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_6_q8 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_7, arith_out_col_out_7_d1, arith_out_col_out_7_d2, arith_out_col_out_7_d3, arith_out_col_out_7_d4, arith_out_col_out_7_d5, arith_out_col_out_7_d6, arith_out_col_out_7_d7 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_7_q7 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_8, arith_out_col_out_8_d1, arith_out_col_out_8_d2, arith_out_col_out_8_d3, arith_out_col_out_8_d4, arith_out_col_out_8_d5, arith_out_col_out_8_d6 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_8_q6 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_9, arith_out_col_out_9_d1, arith_out_col_out_9_d2, arith_out_col_out_9_d3, arith_out_col_out_9_d4, arith_out_col_out_9_d5 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_9_q5 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_10, arith_out_col_out_10_d1, arith_out_col_out_10_d2, arith_out_col_out_10_d3, arith_out_col_out_10_d4 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_10_q4 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_11, arith_out_col_out_11_d1, arith_out_col_out_11_d2, arith_out_col_out_11_d3 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_11_q3 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_12, arith_out_col_out_12_d1, arith_out_col_out_12_d2 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_12_q2 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_13, arith_out_col_out_13_d1 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_13_q1 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_14 :  std_logic_vector(31 downto 0);
signal arith_out_col_out_14_q0 :  std_logic_vector(31 downto 0);
signal rows_i_arith : array_N_dense;
signal rows_i_s3 :  std_logic_vector(543 downto 0);
signal cols_j_arith : array_M_dense;
signal cols_j_s3 :  std_logic_vector(509 downto 0);
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
            arith_in_row_8_d1 <=  arith_in_row_8;
            arith_in_row_8_d2 <=  arith_in_row_8_d1;
            arith_in_row_8_d3 <=  arith_in_row_8_d2;
            arith_in_row_8_d4 <=  arith_in_row_8_d3;
            arith_in_row_8_d5 <=  arith_in_row_8_d4;
            arith_in_row_8_d6 <=  arith_in_row_8_d5;
            arith_in_row_8_d7 <=  arith_in_row_8_d6;
            arith_in_row_8_d8 <=  arith_in_row_8_d7;
            arith_in_row_9_d1 <=  arith_in_row_9;
            arith_in_row_9_d2 <=  arith_in_row_9_d1;
            arith_in_row_9_d3 <=  arith_in_row_9_d2;
            arith_in_row_9_d4 <=  arith_in_row_9_d3;
            arith_in_row_9_d5 <=  arith_in_row_9_d4;
            arith_in_row_9_d6 <=  arith_in_row_9_d5;
            arith_in_row_9_d7 <=  arith_in_row_9_d6;
            arith_in_row_9_d8 <=  arith_in_row_9_d7;
            arith_in_row_9_d9 <=  arith_in_row_9_d8;
            arith_in_row_10_d1 <=  arith_in_row_10;
            arith_in_row_10_d2 <=  arith_in_row_10_d1;
            arith_in_row_10_d3 <=  arith_in_row_10_d2;
            arith_in_row_10_d4 <=  arith_in_row_10_d3;
            arith_in_row_10_d5 <=  arith_in_row_10_d4;
            arith_in_row_10_d6 <=  arith_in_row_10_d5;
            arith_in_row_10_d7 <=  arith_in_row_10_d6;
            arith_in_row_10_d8 <=  arith_in_row_10_d7;
            arith_in_row_10_d9 <=  arith_in_row_10_d8;
            arith_in_row_10_d10 <=  arith_in_row_10_d9;
            arith_in_row_11_d1 <=  arith_in_row_11;
            arith_in_row_11_d2 <=  arith_in_row_11_d1;
            arith_in_row_11_d3 <=  arith_in_row_11_d2;
            arith_in_row_11_d4 <=  arith_in_row_11_d3;
            arith_in_row_11_d5 <=  arith_in_row_11_d4;
            arith_in_row_11_d6 <=  arith_in_row_11_d5;
            arith_in_row_11_d7 <=  arith_in_row_11_d6;
            arith_in_row_11_d8 <=  arith_in_row_11_d7;
            arith_in_row_11_d9 <=  arith_in_row_11_d8;
            arith_in_row_11_d10 <=  arith_in_row_11_d9;
            arith_in_row_11_d11 <=  arith_in_row_11_d10;
            arith_in_row_12_d1 <=  arith_in_row_12;
            arith_in_row_12_d2 <=  arith_in_row_12_d1;
            arith_in_row_12_d3 <=  arith_in_row_12_d2;
            arith_in_row_12_d4 <=  arith_in_row_12_d3;
            arith_in_row_12_d5 <=  arith_in_row_12_d4;
            arith_in_row_12_d6 <=  arith_in_row_12_d5;
            arith_in_row_12_d7 <=  arith_in_row_12_d6;
            arith_in_row_12_d8 <=  arith_in_row_12_d7;
            arith_in_row_12_d9 <=  arith_in_row_12_d8;
            arith_in_row_12_d10 <=  arith_in_row_12_d9;
            arith_in_row_12_d11 <=  arith_in_row_12_d10;
            arith_in_row_12_d12 <=  arith_in_row_12_d11;
            arith_in_row_13_d1 <=  arith_in_row_13;
            arith_in_row_13_d2 <=  arith_in_row_13_d1;
            arith_in_row_13_d3 <=  arith_in_row_13_d2;
            arith_in_row_13_d4 <=  arith_in_row_13_d3;
            arith_in_row_13_d5 <=  arith_in_row_13_d4;
            arith_in_row_13_d6 <=  arith_in_row_13_d5;
            arith_in_row_13_d7 <=  arith_in_row_13_d6;
            arith_in_row_13_d8 <=  arith_in_row_13_d7;
            arith_in_row_13_d9 <=  arith_in_row_13_d8;
            arith_in_row_13_d10 <=  arith_in_row_13_d9;
            arith_in_row_13_d11 <=  arith_in_row_13_d10;
            arith_in_row_13_d12 <=  arith_in_row_13_d11;
            arith_in_row_13_d13 <=  arith_in_row_13_d12;
            arith_in_row_14_d1 <=  arith_in_row_14;
            arith_in_row_14_d2 <=  arith_in_row_14_d1;
            arith_in_row_14_d3 <=  arith_in_row_14_d2;
            arith_in_row_14_d4 <=  arith_in_row_14_d3;
            arith_in_row_14_d5 <=  arith_in_row_14_d4;
            arith_in_row_14_d6 <=  arith_in_row_14_d5;
            arith_in_row_14_d7 <=  arith_in_row_14_d6;
            arith_in_row_14_d8 <=  arith_in_row_14_d7;
            arith_in_row_14_d9 <=  arith_in_row_14_d8;
            arith_in_row_14_d10 <=  arith_in_row_14_d9;
            arith_in_row_14_d11 <=  arith_in_row_14_d10;
            arith_in_row_14_d12 <=  arith_in_row_14_d11;
            arith_in_row_14_d13 <=  arith_in_row_14_d12;
            arith_in_row_14_d14 <=  arith_in_row_14_d13;
            arith_in_row_15_d1 <=  arith_in_row_15;
            arith_in_row_15_d2 <=  arith_in_row_15_d1;
            arith_in_row_15_d3 <=  arith_in_row_15_d2;
            arith_in_row_15_d4 <=  arith_in_row_15_d3;
            arith_in_row_15_d5 <=  arith_in_row_15_d4;
            arith_in_row_15_d6 <=  arith_in_row_15_d5;
            arith_in_row_15_d7 <=  arith_in_row_15_d6;
            arith_in_row_15_d8 <=  arith_in_row_15_d7;
            arith_in_row_15_d9 <=  arith_in_row_15_d8;
            arith_in_row_15_d10 <=  arith_in_row_15_d9;
            arith_in_row_15_d11 <=  arith_in_row_15_d10;
            arith_in_row_15_d12 <=  arith_in_row_15_d11;
            arith_in_row_15_d13 <=  arith_in_row_15_d12;
            arith_in_row_15_d14 <=  arith_in_row_15_d13;
            arith_in_row_15_d15 <=  arith_in_row_15_d14;
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
            arith_in_col_7_d1 <=  arith_in_col_7;
            arith_in_col_7_d2 <=  arith_in_col_7_d1;
            arith_in_col_7_d3 <=  arith_in_col_7_d2;
            arith_in_col_7_d4 <=  arith_in_col_7_d3;
            arith_in_col_7_d5 <=  arith_in_col_7_d4;
            arith_in_col_7_d6 <=  arith_in_col_7_d5;
            arith_in_col_7_d7 <=  arith_in_col_7_d6;
            arith_in_col_8_d1 <=  arith_in_col_8;
            arith_in_col_8_d2 <=  arith_in_col_8_d1;
            arith_in_col_8_d3 <=  arith_in_col_8_d2;
            arith_in_col_8_d4 <=  arith_in_col_8_d3;
            arith_in_col_8_d5 <=  arith_in_col_8_d4;
            arith_in_col_8_d6 <=  arith_in_col_8_d5;
            arith_in_col_8_d7 <=  arith_in_col_8_d6;
            arith_in_col_8_d8 <=  arith_in_col_8_d7;
            arith_in_col_9_d1 <=  arith_in_col_9;
            arith_in_col_9_d2 <=  arith_in_col_9_d1;
            arith_in_col_9_d3 <=  arith_in_col_9_d2;
            arith_in_col_9_d4 <=  arith_in_col_9_d3;
            arith_in_col_9_d5 <=  arith_in_col_9_d4;
            arith_in_col_9_d6 <=  arith_in_col_9_d5;
            arith_in_col_9_d7 <=  arith_in_col_9_d6;
            arith_in_col_9_d8 <=  arith_in_col_9_d7;
            arith_in_col_9_d9 <=  arith_in_col_9_d8;
            arith_in_col_10_d1 <=  arith_in_col_10;
            arith_in_col_10_d2 <=  arith_in_col_10_d1;
            arith_in_col_10_d3 <=  arith_in_col_10_d2;
            arith_in_col_10_d4 <=  arith_in_col_10_d3;
            arith_in_col_10_d5 <=  arith_in_col_10_d4;
            arith_in_col_10_d6 <=  arith_in_col_10_d5;
            arith_in_col_10_d7 <=  arith_in_col_10_d6;
            arith_in_col_10_d8 <=  arith_in_col_10_d7;
            arith_in_col_10_d9 <=  arith_in_col_10_d8;
            arith_in_col_10_d10 <=  arith_in_col_10_d9;
            arith_in_col_11_d1 <=  arith_in_col_11;
            arith_in_col_11_d2 <=  arith_in_col_11_d1;
            arith_in_col_11_d3 <=  arith_in_col_11_d2;
            arith_in_col_11_d4 <=  arith_in_col_11_d3;
            arith_in_col_11_d5 <=  arith_in_col_11_d4;
            arith_in_col_11_d6 <=  arith_in_col_11_d5;
            arith_in_col_11_d7 <=  arith_in_col_11_d6;
            arith_in_col_11_d8 <=  arith_in_col_11_d7;
            arith_in_col_11_d9 <=  arith_in_col_11_d8;
            arith_in_col_11_d10 <=  arith_in_col_11_d9;
            arith_in_col_11_d11 <=  arith_in_col_11_d10;
            arith_in_col_12_d1 <=  arith_in_col_12;
            arith_in_col_12_d2 <=  arith_in_col_12_d1;
            arith_in_col_12_d3 <=  arith_in_col_12_d2;
            arith_in_col_12_d4 <=  arith_in_col_12_d3;
            arith_in_col_12_d5 <=  arith_in_col_12_d4;
            arith_in_col_12_d6 <=  arith_in_col_12_d5;
            arith_in_col_12_d7 <=  arith_in_col_12_d6;
            arith_in_col_12_d8 <=  arith_in_col_12_d7;
            arith_in_col_12_d9 <=  arith_in_col_12_d8;
            arith_in_col_12_d10 <=  arith_in_col_12_d9;
            arith_in_col_12_d11 <=  arith_in_col_12_d10;
            arith_in_col_12_d12 <=  arith_in_col_12_d11;
            arith_in_col_13_d1 <=  arith_in_col_13;
            arith_in_col_13_d2 <=  arith_in_col_13_d1;
            arith_in_col_13_d3 <=  arith_in_col_13_d2;
            arith_in_col_13_d4 <=  arith_in_col_13_d3;
            arith_in_col_13_d5 <=  arith_in_col_13_d4;
            arith_in_col_13_d6 <=  arith_in_col_13_d5;
            arith_in_col_13_d7 <=  arith_in_col_13_d6;
            arith_in_col_13_d8 <=  arith_in_col_13_d7;
            arith_in_col_13_d9 <=  arith_in_col_13_d8;
            arith_in_col_13_d10 <=  arith_in_col_13_d9;
            arith_in_col_13_d11 <=  arith_in_col_13_d10;
            arith_in_col_13_d12 <=  arith_in_col_13_d11;
            arith_in_col_13_d13 <=  arith_in_col_13_d12;
            arith_in_col_14_d1 <=  arith_in_col_14;
            arith_in_col_14_d2 <=  arith_in_col_14_d1;
            arith_in_col_14_d3 <=  arith_in_col_14_d2;
            arith_in_col_14_d4 <=  arith_in_col_14_d3;
            arith_in_col_14_d5 <=  arith_in_col_14_d4;
            arith_in_col_14_d6 <=  arith_in_col_14_d5;
            arith_in_col_14_d7 <=  arith_in_col_14_d6;
            arith_in_col_14_d8 <=  arith_in_col_14_d7;
            arith_in_col_14_d9 <=  arith_in_col_14_d8;
            arith_in_col_14_d10 <=  arith_in_col_14_d9;
            arith_in_col_14_d11 <=  arith_in_col_14_d10;
            arith_in_col_14_d12 <=  arith_in_col_14_d11;
            arith_in_col_14_d13 <=  arith_in_col_14_d12;
            arith_in_col_14_d14 <=  arith_in_col_14_d13;
            arith_out_col_out_0_d1 <=  arith_out_col_out_0;
            arith_out_col_out_0_d2 <=  arith_out_col_out_0_d1;
            arith_out_col_out_0_d3 <=  arith_out_col_out_0_d2;
            arith_out_col_out_0_d4 <=  arith_out_col_out_0_d3;
            arith_out_col_out_0_d5 <=  arith_out_col_out_0_d4;
            arith_out_col_out_0_d6 <=  arith_out_col_out_0_d5;
            arith_out_col_out_0_d7 <=  arith_out_col_out_0_d6;
            arith_out_col_out_0_d8 <=  arith_out_col_out_0_d7;
            arith_out_col_out_0_d9 <=  arith_out_col_out_0_d8;
            arith_out_col_out_0_d10 <=  arith_out_col_out_0_d9;
            arith_out_col_out_0_d11 <=  arith_out_col_out_0_d10;
            arith_out_col_out_0_d12 <=  arith_out_col_out_0_d11;
            arith_out_col_out_0_d13 <=  arith_out_col_out_0_d12;
            arith_out_col_out_0_d14 <=  arith_out_col_out_0_d13;
            arith_out_col_out_1_d1 <=  arith_out_col_out_1;
            arith_out_col_out_1_d2 <=  arith_out_col_out_1_d1;
            arith_out_col_out_1_d3 <=  arith_out_col_out_1_d2;
            arith_out_col_out_1_d4 <=  arith_out_col_out_1_d3;
            arith_out_col_out_1_d5 <=  arith_out_col_out_1_d4;
            arith_out_col_out_1_d6 <=  arith_out_col_out_1_d5;
            arith_out_col_out_1_d7 <=  arith_out_col_out_1_d6;
            arith_out_col_out_1_d8 <=  arith_out_col_out_1_d7;
            arith_out_col_out_1_d9 <=  arith_out_col_out_1_d8;
            arith_out_col_out_1_d10 <=  arith_out_col_out_1_d9;
            arith_out_col_out_1_d11 <=  arith_out_col_out_1_d10;
            arith_out_col_out_1_d12 <=  arith_out_col_out_1_d11;
            arith_out_col_out_1_d13 <=  arith_out_col_out_1_d12;
            arith_out_col_out_2_d1 <=  arith_out_col_out_2;
            arith_out_col_out_2_d2 <=  arith_out_col_out_2_d1;
            arith_out_col_out_2_d3 <=  arith_out_col_out_2_d2;
            arith_out_col_out_2_d4 <=  arith_out_col_out_2_d3;
            arith_out_col_out_2_d5 <=  arith_out_col_out_2_d4;
            arith_out_col_out_2_d6 <=  arith_out_col_out_2_d5;
            arith_out_col_out_2_d7 <=  arith_out_col_out_2_d6;
            arith_out_col_out_2_d8 <=  arith_out_col_out_2_d7;
            arith_out_col_out_2_d9 <=  arith_out_col_out_2_d8;
            arith_out_col_out_2_d10 <=  arith_out_col_out_2_d9;
            arith_out_col_out_2_d11 <=  arith_out_col_out_2_d10;
            arith_out_col_out_2_d12 <=  arith_out_col_out_2_d11;
            arith_out_col_out_3_d1 <=  arith_out_col_out_3;
            arith_out_col_out_3_d2 <=  arith_out_col_out_3_d1;
            arith_out_col_out_3_d3 <=  arith_out_col_out_3_d2;
            arith_out_col_out_3_d4 <=  arith_out_col_out_3_d3;
            arith_out_col_out_3_d5 <=  arith_out_col_out_3_d4;
            arith_out_col_out_3_d6 <=  arith_out_col_out_3_d5;
            arith_out_col_out_3_d7 <=  arith_out_col_out_3_d6;
            arith_out_col_out_3_d8 <=  arith_out_col_out_3_d7;
            arith_out_col_out_3_d9 <=  arith_out_col_out_3_d8;
            arith_out_col_out_3_d10 <=  arith_out_col_out_3_d9;
            arith_out_col_out_3_d11 <=  arith_out_col_out_3_d10;
            arith_out_col_out_4_d1 <=  arith_out_col_out_4;
            arith_out_col_out_4_d2 <=  arith_out_col_out_4_d1;
            arith_out_col_out_4_d3 <=  arith_out_col_out_4_d2;
            arith_out_col_out_4_d4 <=  arith_out_col_out_4_d3;
            arith_out_col_out_4_d5 <=  arith_out_col_out_4_d4;
            arith_out_col_out_4_d6 <=  arith_out_col_out_4_d5;
            arith_out_col_out_4_d7 <=  arith_out_col_out_4_d6;
            arith_out_col_out_4_d8 <=  arith_out_col_out_4_d7;
            arith_out_col_out_4_d9 <=  arith_out_col_out_4_d8;
            arith_out_col_out_4_d10 <=  arith_out_col_out_4_d9;
            arith_out_col_out_5_d1 <=  arith_out_col_out_5;
            arith_out_col_out_5_d2 <=  arith_out_col_out_5_d1;
            arith_out_col_out_5_d3 <=  arith_out_col_out_5_d2;
            arith_out_col_out_5_d4 <=  arith_out_col_out_5_d3;
            arith_out_col_out_5_d5 <=  arith_out_col_out_5_d4;
            arith_out_col_out_5_d6 <=  arith_out_col_out_5_d5;
            arith_out_col_out_5_d7 <=  arith_out_col_out_5_d6;
            arith_out_col_out_5_d8 <=  arith_out_col_out_5_d7;
            arith_out_col_out_5_d9 <=  arith_out_col_out_5_d8;
            arith_out_col_out_6_d1 <=  arith_out_col_out_6;
            arith_out_col_out_6_d2 <=  arith_out_col_out_6_d1;
            arith_out_col_out_6_d3 <=  arith_out_col_out_6_d2;
            arith_out_col_out_6_d4 <=  arith_out_col_out_6_d3;
            arith_out_col_out_6_d5 <=  arith_out_col_out_6_d4;
            arith_out_col_out_6_d6 <=  arith_out_col_out_6_d5;
            arith_out_col_out_6_d7 <=  arith_out_col_out_6_d6;
            arith_out_col_out_6_d8 <=  arith_out_col_out_6_d7;
            arith_out_col_out_7_d1 <=  arith_out_col_out_7;
            arith_out_col_out_7_d2 <=  arith_out_col_out_7_d1;
            arith_out_col_out_7_d3 <=  arith_out_col_out_7_d2;
            arith_out_col_out_7_d4 <=  arith_out_col_out_7_d3;
            arith_out_col_out_7_d5 <=  arith_out_col_out_7_d4;
            arith_out_col_out_7_d6 <=  arith_out_col_out_7_d5;
            arith_out_col_out_7_d7 <=  arith_out_col_out_7_d6;
            arith_out_col_out_8_d1 <=  arith_out_col_out_8;
            arith_out_col_out_8_d2 <=  arith_out_col_out_8_d1;
            arith_out_col_out_8_d3 <=  arith_out_col_out_8_d2;
            arith_out_col_out_8_d4 <=  arith_out_col_out_8_d3;
            arith_out_col_out_8_d5 <=  arith_out_col_out_8_d4;
            arith_out_col_out_8_d6 <=  arith_out_col_out_8_d5;
            arith_out_col_out_9_d1 <=  arith_out_col_out_9;
            arith_out_col_out_9_d2 <=  arith_out_col_out_9_d1;
            arith_out_col_out_9_d3 <=  arith_out_col_out_9_d2;
            arith_out_col_out_9_d4 <=  arith_out_col_out_9_d3;
            arith_out_col_out_9_d5 <=  arith_out_col_out_9_d4;
            arith_out_col_out_10_d1 <=  arith_out_col_out_10;
            arith_out_col_out_10_d2 <=  arith_out_col_out_10_d1;
            arith_out_col_out_10_d3 <=  arith_out_col_out_10_d2;
            arith_out_col_out_10_d4 <=  arith_out_col_out_10_d3;
            arith_out_col_out_11_d1 <=  arith_out_col_out_11;
            arith_out_col_out_11_d2 <=  arith_out_col_out_11_d1;
            arith_out_col_out_11_d3 <=  arith_out_col_out_11_d2;
            arith_out_col_out_12_d1 <=  arith_out_col_out_12;
            arith_out_col_out_12_d2 <=  arith_out_col_out_12_d1;
            arith_out_col_out_13_d1 <=  arith_out_col_out_13;
         end if;
      end process;
------------ Delay depending on row index incoming dense arithmetic ------------
   arith_in_row_0 <= rowsA(31 downto 0);
   arith_in_row_0_q0 <= arith_in_row_0;
   arith_in_row_1 <= rowsA(63 downto 32);
   arith_in_row_1_q1 <= arith_in_row_1_d1;
   arith_in_row_2 <= rowsA(95 downto 64);
   arith_in_row_2_q2 <= arith_in_row_2_d2;
   arith_in_row_3 <= rowsA(127 downto 96);
   arith_in_row_3_q3 <= arith_in_row_3_d3;
   arith_in_row_4 <= rowsA(159 downto 128);
   arith_in_row_4_q4 <= arith_in_row_4_d4;
   arith_in_row_5 <= rowsA(191 downto 160);
   arith_in_row_5_q5 <= arith_in_row_5_d5;
   arith_in_row_6 <= rowsA(223 downto 192);
   arith_in_row_6_q6 <= arith_in_row_6_d6;
   arith_in_row_7 <= rowsA(255 downto 224);
   arith_in_row_7_q7 <= arith_in_row_7_d7;
   arith_in_row_8 <= rowsA(287 downto 256);
   arith_in_row_8_q8 <= arith_in_row_8_d8;
   arith_in_row_9 <= rowsA(319 downto 288);
   arith_in_row_9_q9 <= arith_in_row_9_d9;
   arith_in_row_10 <= rowsA(351 downto 320);
   arith_in_row_10_q10 <= arith_in_row_10_d10;
   arith_in_row_11 <= rowsA(383 downto 352);
   arith_in_row_11_q11 <= arith_in_row_11_d11;
   arith_in_row_12 <= rowsA(415 downto 384);
   arith_in_row_12_q12 <= arith_in_row_12_d12;
   arith_in_row_13 <= rowsA(447 downto 416);
   arith_in_row_13_q13 <= arith_in_row_13_d13;
   arith_in_row_14 <= rowsA(479 downto 448);
   arith_in_row_14_q14 <= arith_in_row_14_d14;
   arith_in_row_15 <= rowsA(511 downto 480);
   arith_in_row_15_q15 <= arith_in_row_15_d15;

------------ Delay depending on col index incoming dense arithmetic ------------
   arith_in_col_0 <= colsB(31 downto 0);
   arith_in_col_0_q0 <= arith_in_col_0;
   arith_in_col_1 <= colsB(63 downto 32);
   arith_in_col_1_q1 <= arith_in_col_1_d1;
   arith_in_col_2 <= colsB(95 downto 64);
   arith_in_col_2_q2 <= arith_in_col_2_d2;
   arith_in_col_3 <= colsB(127 downto 96);
   arith_in_col_3_q3 <= arith_in_col_3_d3;
   arith_in_col_4 <= colsB(159 downto 128);
   arith_in_col_4_q4 <= arith_in_col_4_d4;
   arith_in_col_5 <= colsB(191 downto 160);
   arith_in_col_5_q5 <= arith_in_col_5_d5;
   arith_in_col_6 <= colsB(223 downto 192);
   arith_in_col_6_q6 <= arith_in_col_6_d6;
   arith_in_col_7 <= colsB(255 downto 224);
   arith_in_col_7_q7 <= arith_in_col_7_d7;
   arith_in_col_8 <= colsB(287 downto 256);
   arith_in_col_8_q8 <= arith_in_col_8_d8;
   arith_in_col_9 <= colsB(319 downto 288);
   arith_in_col_9_q9 <= arith_in_col_9_d9;
   arith_in_col_10 <= colsB(351 downto 320);
   arith_in_col_10_q10 <= arith_in_col_10_d10;
   arith_in_col_11 <= colsB(383 downto 352);
   arith_in_col_11_q11 <= arith_in_col_11_d11;
   arith_in_col_12 <= colsB(415 downto 384);
   arith_in_col_12_q12 <= arith_in_col_12_d12;
   arith_in_col_13 <= colsB(447 downto 416);
   arith_in_col_13_q13 <= arith_in_col_13_d13;
   arith_in_col_14 <= colsB(479 downto 448);
   arith_in_col_14_q14 <= arith_in_col_14_d14;

--------------- Delay SOB/EOB with Arith_to_S3 delay to feed SAK ---------------
   SOB_select <= SOB;
   SOB_q0 <= SOB_select;
   EOB_select <= EOB;
   EOB_q0 <= EOB_select;

--------------- Delay outgoing arithmetic depending on col index ---------------
   arith_out_col_out_0 <= LAICPT2_to_arith(31 downto 0);
   arith_out_col_out_0_q14 <= arith_out_col_out_0_d14;
   arith_out_col_out_1 <= LAICPT2_to_arith(63 downto 32);
   arith_out_col_out_1_q13 <= arith_out_col_out_1_d13;
   arith_out_col_out_2 <= LAICPT2_to_arith(95 downto 64);
   arith_out_col_out_2_q12 <= arith_out_col_out_2_d12;
   arith_out_col_out_3 <= LAICPT2_to_arith(127 downto 96);
   arith_out_col_out_3_q11 <= arith_out_col_out_3_d11;
   arith_out_col_out_4 <= LAICPT2_to_arith(159 downto 128);
   arith_out_col_out_4_q10 <= arith_out_col_out_4_d10;
   arith_out_col_out_5 <= LAICPT2_to_arith(191 downto 160);
   arith_out_col_out_5_q9 <= arith_out_col_out_5_d9;
   arith_out_col_out_6 <= LAICPT2_to_arith(223 downto 192);
   arith_out_col_out_6_q8 <= arith_out_col_out_6_d8;
   arith_out_col_out_7 <= LAICPT2_to_arith(255 downto 224);
   arith_out_col_out_7_q7 <= arith_out_col_out_7_d7;
   arith_out_col_out_8 <= LAICPT2_to_arith(287 downto 256);
   arith_out_col_out_8_q6 <= arith_out_col_out_8_d6;
   arith_out_col_out_9 <= LAICPT2_to_arith(319 downto 288);
   arith_out_col_out_9_q5 <= arith_out_col_out_9_d5;
   arith_out_col_out_10 <= LAICPT2_to_arith(351 downto 320);
   arith_out_col_out_10_q4 <= arith_out_col_out_10_d4;
   arith_out_col_out_11 <= LAICPT2_to_arith(383 downto 352);
   arith_out_col_out_11_q3 <= arith_out_col_out_11_d3;
   arith_out_col_out_12 <= LAICPT2_to_arith(415 downto 384);
   arith_out_col_out_12_q2 <= arith_out_col_out_12_d2;
   arith_out_col_out_13 <= LAICPT2_to_arith(447 downto 416);
   arith_out_col_out_13_q1 <= arith_out_col_out_13_d1;
   arith_out_col_out_14 <= LAICPT2_to_arith(479 downto 448);
   arith_out_col_out_14_q0 <= arith_out_col_out_14;

---------------- Generate Arith_to_S3 for rows and connect them ----------------
   rows_i_arith(0) <= arith_in_row_0_q0;
   rows_i_arith(1) <= arith_in_row_1_q1;
   rows_i_arith(2) <= arith_in_row_2_q2;
   rows_i_arith(3) <= arith_in_row_3_q3;
   rows_i_arith(4) <= arith_in_row_4_q4;
   rows_i_arith(5) <= arith_in_row_5_q5;
   rows_i_arith(6) <= arith_in_row_6_q6;
   rows_i_arith(7) <= arith_in_row_7_q7;
   rows_i_arith(8) <= arith_in_row_8_q8;
   rows_i_arith(9) <= arith_in_row_9_q9;
   rows_i_arith(10) <= arith_in_row_10_q10;
   rows_i_arith(11) <= arith_in_row_11_q11;
   rows_i_arith(12) <= arith_in_row_12_q12;
   rows_i_arith(13) <= arith_in_row_13_q13;
   rows_i_arith(14) <= arith_in_row_14_q14;
   rows_i_arith(15) <= arith_in_row_15_q15;
   rows_a2s3: for II in 0 to 15 generate
      a2s3_i: Arith_to_S3
         port map ( clk => clk,
                    arith_i => rows_i_arith(II),
                    s3_o => rows_i_s3(((II+1)*34)-1 downto II*34));
   end generate;

---------------- Generate Arith_to_S3 for cols and connect them ----------------
   cols_j_arith(0) <= arith_in_col_0_q0;
   cols_j_arith(1) <= arith_in_col_1_q1;
   cols_j_arith(2) <= arith_in_col_2_q2;
   cols_j_arith(3) <= arith_in_col_3_q3;
   cols_j_arith(4) <= arith_in_col_4_q4;
   cols_j_arith(5) <= arith_in_col_5_q5;
   cols_j_arith(6) <= arith_in_col_6_q6;
   cols_j_arith(7) <= arith_in_col_7_q7;
   cols_j_arith(8) <= arith_in_col_8_q8;
   cols_j_arith(9) <= arith_in_col_9_q9;
   cols_j_arith(10) <= arith_in_col_10_q10;
   cols_j_arith(11) <= arith_in_col_11_q11;
   cols_j_arith(12) <= arith_in_col_12_q12;
   cols_j_arith(13) <= arith_in_col_13_q13;
   cols_j_arith(14) <= arith_in_col_14_q14;
   cols_a2s3: for JJ in 0 to 14 generate
      a2s3_j: Arith_to_S3
         port map ( clk => clk,
                    arith_i => cols_j_arith(JJ),
                    s3_o => cols_j_s3(((JJ+1)*34)-1 downto JJ*34));
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
   cols_l2a: for JJ in 0 to 14 generate
      l2a_i: l2a
         port map ( clk => clk,
                    A => colsC_LAICPT2(((JJ+1)*33)-1-1-0 downto JJ*33),
                    isNaN => colsC_LAICPT2(((JJ+1)*33)- 1),
                    arith_o => LAICPT2_to_arith(((JJ+1)*32)-1 downto JJ*32));
   end generate;

-------- Connect outgoing delayed dense arith words to colsC output bus --------
   colsC(31 downto 0) <= arith_out_col_out_0_q14;
   colsC(63 downto 32) <= arith_out_col_out_1_q13;
   colsC(95 downto 64) <= arith_out_col_out_2_q12;
   colsC(127 downto 96) <= arith_out_col_out_3_q11;
   colsC(159 downto 128) <= arith_out_col_out_4_q10;
   colsC(191 downto 160) <= arith_out_col_out_5_q9;
   colsC(223 downto 192) <= arith_out_col_out_6_q8;
   colsC(255 downto 224) <= arith_out_col_out_7_q7;
   colsC(287 downto 256) <= arith_out_col_out_8_q6;
   colsC(319 downto 288) <= arith_out_col_out_9_q5;
   colsC(351 downto 320) <= arith_out_col_out_10_q4;
   colsC(383 downto 352) <= arith_out_col_out_11_q3;
   colsC(415 downto 384) <= arith_out_col_out_12_q2;
   colsC(447 downto 416) <= arith_out_col_out_13_q1;
   colsC(479 downto 448) <= arith_out_col_out_14_q0;

end architecture;

