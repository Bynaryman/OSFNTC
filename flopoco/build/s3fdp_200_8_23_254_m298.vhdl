--------------------------------------------------------------------------------
--                          DSPBlock_17x24_F200_uid8
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

entity DSPBlock_17x24_F200_uid8 is
    port (clk : in std_logic;
          X : in  std_logic_vector(16 downto 0);
          Y : in  std_logic_vector(23 downto 0);
          R : out  std_logic_vector(40 downto 0)   );
end entity;

architecture arch of DSPBlock_17x24_F200_uid8 is
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
--                          DSPBlock_7x24_F200_uid10
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

entity DSPBlock_7x24_F200_uid10 is
    port (clk : in std_logic;
          X : in  std_logic_vector(6 downto 0);
          Y : in  std_logic_vector(23 downto 0);
          R : out  std_logic_vector(30 downto 0)   );
end entity;

architecture arch of DSPBlock_7x24_F200_uid10 is
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
--                           IntAdder_32_F200_uid13
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

entity IntAdder_32_F200_uid13 is
    port (clk : in std_logic;
          X : in  std_logic_vector(31 downto 0);
          Y : in  std_logic_vector(31 downto 0);
          Cin : in  std_logic;
          R : out  std_logic_vector(31 downto 0)   );
end entity;

architecture arch of IntAdder_32_F200_uid13 is
signal Rtmp :  std_logic_vector(31 downto 0);
begin
   Rtmp <= X + Y + Cin;
   R <= Rtmp;
end architecture;

--------------------------------------------------------------------------------
--                          IntMultiplier_F200_uid4
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

entity IntMultiplier_F200_uid4 is
    port (clk : in std_logic;
          X : in  std_logic_vector(23 downto 0);
          Y : in  std_logic_vector(23 downto 0);
          R : out  std_logic_vector(47 downto 0)   );
end entity;

architecture arch of IntMultiplier_F200_uid4 is
   component DSPBlock_17x24_F200_uid8 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(16 downto 0);
             Y : in  std_logic_vector(23 downto 0);
             R : out  std_logic_vector(40 downto 0)   );
   end component;

   component DSPBlock_7x24_F200_uid10 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(6 downto 0);
             Y : in  std_logic_vector(23 downto 0);
             R : out  std_logic_vector(30 downto 0)   );
   end component;

   component IntAdder_32_F200_uid13 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(31 downto 0);
             Y : in  std_logic_vector(31 downto 0);
             Cin : in  std_logic;
             R : out  std_logic_vector(31 downto 0)   );
   end component;

signal XX_m5 :  std_logic_vector(23 downto 0);
signal YY_m5 :  std_logic_vector(23 downto 0);
signal tile_0_X :  std_logic_vector(16 downto 0);
signal tile_0_Y :  std_logic_vector(23 downto 0);
signal tile_0_output :  std_logic_vector(40 downto 0);
signal tile_0_filtered_output :  std_logic_vector(40 downto 0);
signal bh6_w0_0 :  std_logic;
signal bh6_w1_0 :  std_logic;
signal bh6_w2_0 :  std_logic;
signal bh6_w3_0 :  std_logic;
signal bh6_w4_0 :  std_logic;
signal bh6_w5_0 :  std_logic;
signal bh6_w6_0 :  std_logic;
signal bh6_w7_0 :  std_logic;
signal bh6_w8_0 :  std_logic;
signal bh6_w9_0 :  std_logic;
signal bh6_w10_0 :  std_logic;
signal bh6_w11_0 :  std_logic;
signal bh6_w12_0 :  std_logic;
signal bh6_w13_0 :  std_logic;
signal bh6_w14_0 :  std_logic;
signal bh6_w15_0 :  std_logic;
signal bh6_w16_0 :  std_logic;
signal bh6_w17_0 :  std_logic;
signal bh6_w18_0 :  std_logic;
signal bh6_w19_0 :  std_logic;
signal bh6_w20_0 :  std_logic;
signal bh6_w21_0 :  std_logic;
signal bh6_w22_0 :  std_logic;
signal bh6_w23_0 :  std_logic;
signal bh6_w24_0 :  std_logic;
signal bh6_w25_0 :  std_logic;
signal bh6_w26_0 :  std_logic;
signal bh6_w27_0 :  std_logic;
signal bh6_w28_0 :  std_logic;
signal bh6_w29_0 :  std_logic;
signal bh6_w30_0 :  std_logic;
signal bh6_w31_0 :  std_logic;
signal bh6_w32_0 :  std_logic;
signal bh6_w33_0 :  std_logic;
signal bh6_w34_0 :  std_logic;
signal bh6_w35_0 :  std_logic;
signal bh6_w36_0 :  std_logic;
signal bh6_w37_0 :  std_logic;
signal bh6_w38_0 :  std_logic;
signal bh6_w39_0 :  std_logic;
signal bh6_w40_0 :  std_logic;
signal tile_1_X :  std_logic_vector(6 downto 0);
signal tile_1_Y :  std_logic_vector(23 downto 0);
signal tile_1_output :  std_logic_vector(30 downto 0);
signal tile_1_filtered_output :  std_logic_vector(30 downto 0);
signal bh6_w17_1 :  std_logic;
signal bh6_w18_1 :  std_logic;
signal bh6_w19_1 :  std_logic;
signal bh6_w20_1 :  std_logic;
signal bh6_w21_1 :  std_logic;
signal bh6_w22_1 :  std_logic;
signal bh6_w23_1 :  std_logic;
signal bh6_w24_1 :  std_logic;
signal bh6_w25_1 :  std_logic;
signal bh6_w26_1 :  std_logic;
signal bh6_w27_1 :  std_logic;
signal bh6_w28_1 :  std_logic;
signal bh6_w29_1 :  std_logic;
signal bh6_w30_1 :  std_logic;
signal bh6_w31_1 :  std_logic;
signal bh6_w32_1 :  std_logic;
signal bh6_w33_1 :  std_logic;
signal bh6_w34_1 :  std_logic;
signal bh6_w35_1 :  std_logic;
signal bh6_w36_1 :  std_logic;
signal bh6_w37_1 :  std_logic;
signal bh6_w38_1 :  std_logic;
signal bh6_w39_1 :  std_logic;
signal bh6_w40_1 :  std_logic;
signal bh6_w41_0 :  std_logic;
signal bh6_w42_0 :  std_logic;
signal bh6_w43_0 :  std_logic;
signal bh6_w44_0 :  std_logic;
signal bh6_w45_0 :  std_logic;
signal bh6_w46_0 :  std_logic;
signal bh6_w47_0 :  std_logic;
signal tmp_bitheapResult_bh6_16 :  std_logic_vector(16 downto 0);
signal bitheapFinalAdd_bh6_In0 :  std_logic_vector(31 downto 0);
signal bitheapFinalAdd_bh6_In1 :  std_logic_vector(31 downto 0);
signal bitheapFinalAdd_bh6_Cin :  std_logic;
signal bitheapFinalAdd_bh6_Out :  std_logic_vector(31 downto 0);
signal bitheapResult_bh6 :  std_logic_vector(47 downto 0);
begin
   XX_m5 <= X ;
   YY_m5 <= Y ;
   tile_0_X <= X(16 downto 0);
   tile_0_Y <= Y(23 downto 0);
   tile_0_mult: DSPBlock_17x24_F200_uid8
      port map ( clk  => clk,
                 X => tile_0_X,
                 Y => tile_0_Y,
                 R => tile_0_output);

tile_0_filtered_output <= tile_0_output(40 downto 0);
   bh6_w0_0 <= tile_0_filtered_output(0);
   bh6_w1_0 <= tile_0_filtered_output(1);
   bh6_w2_0 <= tile_0_filtered_output(2);
   bh6_w3_0 <= tile_0_filtered_output(3);
   bh6_w4_0 <= tile_0_filtered_output(4);
   bh6_w5_0 <= tile_0_filtered_output(5);
   bh6_w6_0 <= tile_0_filtered_output(6);
   bh6_w7_0 <= tile_0_filtered_output(7);
   bh6_w8_0 <= tile_0_filtered_output(8);
   bh6_w9_0 <= tile_0_filtered_output(9);
   bh6_w10_0 <= tile_0_filtered_output(10);
   bh6_w11_0 <= tile_0_filtered_output(11);
   bh6_w12_0 <= tile_0_filtered_output(12);
   bh6_w13_0 <= tile_0_filtered_output(13);
   bh6_w14_0 <= tile_0_filtered_output(14);
   bh6_w15_0 <= tile_0_filtered_output(15);
   bh6_w16_0 <= tile_0_filtered_output(16);
   bh6_w17_0 <= tile_0_filtered_output(17);
   bh6_w18_0 <= tile_0_filtered_output(18);
   bh6_w19_0 <= tile_0_filtered_output(19);
   bh6_w20_0 <= tile_0_filtered_output(20);
   bh6_w21_0 <= tile_0_filtered_output(21);
   bh6_w22_0 <= tile_0_filtered_output(22);
   bh6_w23_0 <= tile_0_filtered_output(23);
   bh6_w24_0 <= tile_0_filtered_output(24);
   bh6_w25_0 <= tile_0_filtered_output(25);
   bh6_w26_0 <= tile_0_filtered_output(26);
   bh6_w27_0 <= tile_0_filtered_output(27);
   bh6_w28_0 <= tile_0_filtered_output(28);
   bh6_w29_0 <= tile_0_filtered_output(29);
   bh6_w30_0 <= tile_0_filtered_output(30);
   bh6_w31_0 <= tile_0_filtered_output(31);
   bh6_w32_0 <= tile_0_filtered_output(32);
   bh6_w33_0 <= tile_0_filtered_output(33);
   bh6_w34_0 <= tile_0_filtered_output(34);
   bh6_w35_0 <= tile_0_filtered_output(35);
   bh6_w36_0 <= tile_0_filtered_output(36);
   bh6_w37_0 <= tile_0_filtered_output(37);
   bh6_w38_0 <= tile_0_filtered_output(38);
   bh6_w39_0 <= tile_0_filtered_output(39);
   bh6_w40_0 <= tile_0_filtered_output(40);
   tile_1_X <= X(23 downto 17);
   tile_1_Y <= Y(23 downto 0);
   tile_1_mult: DSPBlock_7x24_F200_uid10
      port map ( clk  => clk,
                 X => tile_1_X,
                 Y => tile_1_Y,
                 R => tile_1_output);

tile_1_filtered_output <= tile_1_output(30 downto 0);
   bh6_w17_1 <= tile_1_filtered_output(0);
   bh6_w18_1 <= tile_1_filtered_output(1);
   bh6_w19_1 <= tile_1_filtered_output(2);
   bh6_w20_1 <= tile_1_filtered_output(3);
   bh6_w21_1 <= tile_1_filtered_output(4);
   bh6_w22_1 <= tile_1_filtered_output(5);
   bh6_w23_1 <= tile_1_filtered_output(6);
   bh6_w24_1 <= tile_1_filtered_output(7);
   bh6_w25_1 <= tile_1_filtered_output(8);
   bh6_w26_1 <= tile_1_filtered_output(9);
   bh6_w27_1 <= tile_1_filtered_output(10);
   bh6_w28_1 <= tile_1_filtered_output(11);
   bh6_w29_1 <= tile_1_filtered_output(12);
   bh6_w30_1 <= tile_1_filtered_output(13);
   bh6_w31_1 <= tile_1_filtered_output(14);
   bh6_w32_1 <= tile_1_filtered_output(15);
   bh6_w33_1 <= tile_1_filtered_output(16);
   bh6_w34_1 <= tile_1_filtered_output(17);
   bh6_w35_1 <= tile_1_filtered_output(18);
   bh6_w36_1 <= tile_1_filtered_output(19);
   bh6_w37_1 <= tile_1_filtered_output(20);
   bh6_w38_1 <= tile_1_filtered_output(21);
   bh6_w39_1 <= tile_1_filtered_output(22);
   bh6_w40_1 <= tile_1_filtered_output(23);
   bh6_w41_0 <= tile_1_filtered_output(24);
   bh6_w42_0 <= tile_1_filtered_output(25);
   bh6_w43_0 <= tile_1_filtered_output(26);
   bh6_w44_0 <= tile_1_filtered_output(27);
   bh6_w45_0 <= tile_1_filtered_output(28);
   bh6_w46_0 <= tile_1_filtered_output(29);
   bh6_w47_0 <= tile_1_filtered_output(30);

   -- Adding the constant bits
      -- All the constant bits are zero, nothing to add

   tmp_bitheapResult_bh6_16 <= bh6_w16_0 & bh6_w15_0 & bh6_w14_0 & bh6_w13_0 & bh6_w12_0 & bh6_w11_0 & bh6_w10_0 & bh6_w9_0 & bh6_w8_0 & bh6_w7_0 & bh6_w6_0 & bh6_w5_0 & bh6_w4_0 & bh6_w3_0 & bh6_w2_0 & bh6_w1_0 & bh6_w0_0;

   bitheapFinalAdd_bh6_In0 <= "0" & bh6_w47_0 & bh6_w46_0 & bh6_w45_0 & bh6_w44_0 & bh6_w43_0 & bh6_w42_0 & bh6_w41_0 & bh6_w40_0 & bh6_w39_0 & bh6_w38_0 & bh6_w37_0 & bh6_w36_0 & bh6_w35_0 & bh6_w34_0 & bh6_w33_0 & bh6_w32_0 & bh6_w31_0 & bh6_w30_0 & bh6_w29_0 & bh6_w28_0 & bh6_w27_0 & bh6_w26_0 & bh6_w25_0 & bh6_w24_0 & bh6_w23_0 & bh6_w22_0 & bh6_w21_0 & bh6_w20_0 & bh6_w19_0 & bh6_w18_0 & bh6_w17_0;
   bitheapFinalAdd_bh6_In1 <= "0" & "0" & "0" & "0" & "0" & "0" & "0" & "0" & bh6_w40_1 & bh6_w39_1 & bh6_w38_1 & bh6_w37_1 & bh6_w36_1 & bh6_w35_1 & bh6_w34_1 & bh6_w33_1 & bh6_w32_1 & bh6_w31_1 & bh6_w30_1 & bh6_w29_1 & bh6_w28_1 & bh6_w27_1 & bh6_w26_1 & bh6_w25_1 & bh6_w24_1 & bh6_w23_1 & bh6_w22_1 & bh6_w21_1 & bh6_w20_1 & bh6_w19_1 & bh6_w18_1 & bh6_w17_1;
   bitheapFinalAdd_bh6_Cin <= '0';

   bitheapFinalAdd_bh6: IntAdder_32_F200_uid13
      port map ( clk  => clk,
                 Cin => bitheapFinalAdd_bh6_Cin,
                 X => bitheapFinalAdd_bh6_In0,
                 Y => bitheapFinalAdd_bh6_In1,
                 R => bitheapFinalAdd_bh6_Out);
   bitheapResult_bh6 <= bitheapFinalAdd_bh6_Out(30 downto 0) & tmp_bitheapResult_bh6_16;
   R <= bitheapResult_bh6(47 downto 0);
end architecture;

--------------------------------------------------------------------------------
--                    LeftShifter48_by_max_511_F200_uid15
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

entity LeftShifter48_by_max_511_F200_uid15 is
    port (clk : in std_logic;
          X : in  std_logic_vector(47 downto 0);
          S : in  std_logic_vector(8 downto 0);
          padBit : in  std_logic;
          R : out  std_logic_vector(558 downto 0)   );
end entity;

architecture arch of LeftShifter48_by_max_511_F200_uid15 is
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
   R <= level9(558 downto 0);
   level2<= level1 & (1 downto 0 => '0') when ps(1)= '1' else     (1 downto 0 => padBit) & level1;
   R <= level9(558 downto 0);
   level3<= level2 & (3 downto 0 => '0') when ps(2)= '1' else     (3 downto 0 => padBit) & level2;
   R <= level9(558 downto 0);
   level4<= level3 & (7 downto 0 => '0') when ps(3)= '1' else     (7 downto 0 => padBit) & level3;
   R <= level9(558 downto 0);
   level5<= level4 & (15 downto 0 => '0') when ps(4)= '1' else     (15 downto 0 => padBit) & level4;
   R <= level9(558 downto 0);
   level6<= level5_d1 & (31 downto 0 => '0') when ps_d1(5)= '1' else     (31 downto 0 => padBit_d1) & level5_d1;
   R <= level9(558 downto 0);
   level7<= level6 & (63 downto 0 => '0') when ps_d1(6)= '1' else     (63 downto 0 => padBit_d1) & level6;
   R <= level9(558 downto 0);
   level8<= level7_d1 & (127 downto 0 => '0') when ps_d2(7)= '1' else     (127 downto 0 => padBit_d2) & level7_d1;
   R <= level9(558 downto 0);
   level9<= level8 & (255 downto 0 => '0') when ps_d2(8)= '1' else     (255 downto 0 => padBit_d2) & level8;
   R <= level9(558 downto 0);
end architecture;

--------------------------------------------------------------------------------
--                         S3FDP_8_23_HSDD_F200_uid2
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

entity S3FDP_8_23_HSDD_F200_uid2 is
    port (clk, rst : in std_logic;
          S3_x : in  std_logic_vector(33 downto 0);
          S3_y : in  std_logic_vector(33 downto 0);
          FTZ : in  std_logic;
          EOB : in  std_logic;
          A : out  std_logic_vector(559 downto 0);
          EOB_Q : out  std_logic;
          isNaN : out  std_logic   );
end entity;

architecture arch of S3FDP_8_23_HSDD_F200_uid2 is
   component IntMultiplier_F200_uid4 is
      port ( clk : in std_logic;
             X : in  std_logic_vector(23 downto 0);
             Y : in  std_logic_vector(23 downto 0);
             R : out  std_logic_vector(47 downto 0)   );
   end component;

   component LeftShifter48_by_max_511_F200_uid15 is
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
signal summand1c :  std_logic_vector(511 downto 0);
signal ext_summand1c :  std_logic_vector(559 downto 0);
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
signal summand_0 :  std_logic_vector(559 downto 0);
signal summand_and_carry_0 :  std_logic_vector(560 downto 0);
signal acc_0, acc_0_d1 :  std_logic_vector(560 downto 0);
signal acc_0_q :  std_logic_vector(560 downto 0);
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
   significand_product_inst: IntMultiplier_F200_uid4
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
   shift_value <= scale_product_twice_biased - (-45);
   significand_product_shifter_inst: LeftShifter48_by_max_511_F200_uid15
      port map ( clk  => clk,
                 S => shift_value,
                 X => significand_product_cpt1,
                 padBit => sign_M,
                 R => shifted_significand);

-------------- detect too low scale for this specific scratchpad --------------
   too_small <= '1' when (shift_value(8)='1') else '0';

-------------- detect too big scale for this specific scratchpad --------------
   too_big <= '1' when (signed(shift_value) > 552) else '0';

--------------- shifted significand part select to form summand ---------------
   summand1c <= shifted_significand(558 downto 47);
   ext_summand1c <= "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" when too_small_d2='1' else ((559 downto 512 => sign_M_d2) & summand1c);

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
   summand_0 <= ext_summand1c(559 downto 0);
   summand_and_carry_0 <= ("0" & summand_0) + carry_0;
   acc_0 <= (("0" & acc_0_q(559 downto 0)) + summand_and_carry_0) when (not_ftz_sync='1') else
            summand_and_carry_0;

-------------------------------- Output Compose --------------------------------
   A <= acc_0_q(559 downto 0);
   EOB_Q <= EOB_internal_delayed;
   isNaN <= isNaN_delayed;
end architecture;

