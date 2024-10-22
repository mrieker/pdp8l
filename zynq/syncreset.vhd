--    Copyright (C) Mike Rieker, Beverly, MA USA
--    www.outerworldapps.com
--
--    This program is free software; you can redistribute it and/or modify
--    it under the terms of the GNU General Public License as published by
--    the Free Software Foundation; version 2 of the License.
--
--    This program is distributed in the hope that it will be useful,
--    but WITHOUT ANY WARRANTY; without even the implied warranty of
--    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--    GNU General Public License for more details.
--
--    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
--
--    You should have received a copy of the GNU General Public License
--    along with this program; if not, write to the Free Software
--    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
--
--    http://www.gnu.org/licenses/gpl-2.0.html

-- convert asynchronous reset to synchronous reset
-- assertion is passed along immediately
-- negation is passed along just after a rising clock edge

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity SyncReset is
    Port (  CLOCK : in STD_LOGIC;
            RESET_N : in STD_LOGIC;
            SYNCRES_N : out STD_LOGIC);
end SyncReset;

architecture rtl of SyncReset is

    signal debounce : STD_LOGIC;

begin

    process (CLOCK, RESET_N)
    begin
        if RESET_N = '0' then
            SYNCRES_N <= '0';
            debounce <= '0';
        elsif rising_edge (CLOCK) then
            SYNCRES_N <= debounce;
            debounce <= '1';
        end if;
    end process;

end rtl;
