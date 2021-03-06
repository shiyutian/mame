// license:BSD-3-Clause
// copyright-holders:Phil Stroffolino
/*
DJ Boy (c)1989 Kaneko

Hardware has many similarities to Airbusters.

Self Test has two parts:
1) color test : press button#3 to advance past color pattern
2) i/o and sound test: use buttons 1,2,3 to select and play sound/music

- CPU0 manages sprites, which are also used to display text
        irq (0x10) - timing/watchdog
        irq (0x30) - processes sprites
        nmi: wakes up this CPU

- CPU1 manages the protection device, palette, and tilemap(s)
        nmi: resets this CPU
        irq: game update

- CPU2 manages sound chips
        irq: update music
        nmi: handle sound command

- The "BEAST" protection device has access to DIP switches and player inputs.


PCB Layout
----------

BS
|----------------------------------------------|
| 6264               BS15   6116               |
|                    BS-101 6116               |
| BS-005             780C-2                    |
| BS-004                           DSW1 DSW2   |
|                    6264              IO-JAMMA|
| 16MHz                     PAL1|----------|   |
| 12MHz                         |  BEAST   |  J|
|                               |----------|  A|
| BS-003              *                       M|
|            6116     BS-203  6295     YM2203 M|
| BS-000                           YM3014     A|
|          |-------|          6295  324  4558  |
| BS-001   |KANEKO |  780C-2        324        |
|          |PANDORA|                    VOL  JP|
| BS-002   |       |  BS-100  PAL2   324       |
|          |-------|           6264     VOL CN1|
| BS07     4464 4464  BS19     BS-200   LA4460 |
|          4464 4464  PAL3     D780C-2  LA4460 |
|----------------------------------------------|

Notes:
      D780C-2 - Z80 CPU. Clock 6.000MHz [12/2] (for all 3 Z80 CPUs)
      BEAST   - OKI MSM80C51F microcontroller with internal ROM. Clock 6.000MHz on pins 18 & 19
                chip is stamped 'KANEKO Beast (C)Intel '80 (C)KANEKO 1988'
      YM2203  - Yamaha YM2203, clock 3.000MHz [12/4]
      6295    - OKI M6295, clock 1.500MHz [12/8]. Sample rate (Hz) = 12000000 / 8 / 165
      PANDORA - Custom Kaneko graphics generator chip stamped 'PX79C480FP-3 PANDORA-CHIP' (QFP160)
      4464    - 64k x4 DRAM (DIP18)
      6116    - 2k x8 SRAM (DIP24)
      6264    - 8k x8 SRAM (DIP28)
      VSync   - 57.5Hz
      HSync   - 15.68kHz
      JP      - 3 pin jumper to set mono/stereo sound output
      CN1     - 4 pin connector for speakers when jumper is set for stereo sound output
      PAL1    - PAL16L8 stamped 'BS-501'
      PAL2    - PAL16L8 stamped 'BS-502'
      PAL3    - PAL16L8 stamped 'BS-500'
      IO-JAMMA- Custom Kaneko ceramic I/O input resistor pack stamped 'I/O JAMMA MC-8282837'
      LA4460  - Sanyo 12W Power Amplifier (SIL10)
      *       - Unpopulated DIP32 position
      ROMs    -
                BS15.6Y    27C512 EPROM (DIP28)   \ There is an alt. set of labels used for these ROMs with an 'S'
                BS07.1B    27C512 EPROM (DIP28)   | added to the name (i.e. 'BS15S'), but the actual ROM contents is identical
                BS19.4B    27C1001 EPROM (DIP32)  / to the regular set (both sets dumped / verified)
                BS-000.1H  4M MASKROM (DIP32) {sprite}
                BS-001.1F  4M MASKROM (DIP32) {sprite}
                BS-002.1D  4M MASKROM (DIP32) {sprite}
                BS-003.1K  4M MASKROM (DIP32) {sprite}
                BS-004.1S  4M MASKROM (DIP32) {tile}
                BS-005.1U  4M MASKROM (DIP32) {tile}
                BS-100.4D  1M MASKROM (DIP28) {z80}
                BS-101.6W  1M MASKROM (DIP28) {z80 data}
                BS-200.8C  1M MASKROM (DIP28) {z80}
                BS-203.5J  2M MASKROM (DIP32) {oki-m6295 samples}

      DIPs    - SW1
                |--------------------------------------------|
                |              1   2   3   4   5   6   7   8 |
                |--------------------------------------------|
                |SCREEN NORMAL    OFF                        |
                |       FLIP      ON                         |
                |--------------------------------------------|
                |GAME   NORMAL        OFF                    |
                |MODE   TEST          ON                     |
                |--------------------------------------------|
                |COIN1  1C/1P                 OFF OFF        |
                |       1C/2P                 ON  OFF        |
                |       2C/1P                 OFF ON         |
                |       2C/3P                 ON  ON         |
                |                                            |
                |COIN2  1C/1P                         OFF OFF|
                |       1C/2P                         ON  OFF|
                |       2C/1P                         OFF ON |
                |       2C/3P                         ON  ON |
                |--------------------------------------------|
                |SW1 & SW4 NOT USED ALWAYS OFF               |
                |--------------------------------------------|

                SW2
                |--------------------------------------------|
                |              1   2   3   4   5   6   7   8 |
                |--------------------------------------------|
                |DIFFICULTY                                  |
                |NORMAL       OFF OFF                        |
                |EASY         ON  OFF                        |
                |HARD         OFF ON                         |
                |HARDEST      ON  ON                         |
                |--------------------------------------------|
                |BONUS                                       |
                |10,30,50,70,90       OFF OFF                |
                |10,20,30,40,50,                             |
                |60,70,80,90          ON  OFF                |
                |20,50                OFF ON                 |
                |NONE                 ON  ON                 |
                |--------------------------------------------|
                |LIVES    5                   OFF OFF        |
                |         3                   ON  OFF        |
                |         7                   OFF ON         |
                |         9                   ON  ON         |
                |--------------------------------------------|
                |DEMO SOUND  YES                      OFF    |
                |            NO                       ON     |
                |--------------------------------------------|
                |SPEAKER     STEREO                       OFF|
                |OUTPUT      MONO                          ON|
                |--------------------------------------------|
*/

#include "emu.h"
#include "includes/djboy.h"

#include "cpu/z80/z80.h"
#include "cpu/mcs51/mcs51.h"
#include "sound/2203intf.h"
#include "sound/okim6295.h"
#include "screen.h"
#include "speaker.h"


/* KANEKO BEAST state */

READ8_MEMBER(djboy_state::beast_status_r)
{
	return (m_slavelatch->pending_r() ? 0x0 : 0x4) | (m_beastlatch->pending_r() ? 0x8 : 0x0);
}

/******************************************************************************/

WRITE8_MEMBER(djboy_state::trigger_nmi_on_mastercpu)
{
	m_mastercpu->set_input_line(INPUT_LINE_NMI, PULSE_LINE);
}

WRITE8_MEMBER(djboy_state::mastercpu_bankswitch_w)
{
	data ^= m_bankxor;
	m_masterbank->set_entry(data);
	m_masterbank_l->set_entry(0); /* unsure if/how this area is banked */
}

/******************************************************************************/

/**
 * xx------ msb scrollx
 * --x----- msb scrolly
 * ---x---- screen flip
 * ----xxxx bank
 */
WRITE8_MEMBER(djboy_state::slavecpu_bankswitch_w)
{
	m_videoreg = data;

	if ((data & 0xc) != 4)
		m_slavebank->set_entry((data & 0xf));
}

WRITE8_MEMBER(djboy_state::coin_count_w)
{
	machine().bookkeeping().coin_counter_w(0, data & 1);
	machine().bookkeeping().coin_counter_w(1, data & 2);
}

/******************************************************************************/

WRITE8_MEMBER(djboy_state::soundcpu_bankswitch_w)
{
	m_soundbank->set_entry(data);  // shall we check data<0x07?
}

/******************************************************************************/

void djboy_state::mastercpu_am(address_map &map)
{
	map(0x0000, 0x7fff).rom();
	map(0x8000, 0xafff).bankr("master_bank_l");
	map(0xb000, 0xbfff).rw(m_pandora, FUNC(kaneko_pandora_device::spriteram_r), FUNC(kaneko_pandora_device::spriteram_w));
	map(0xc000, 0xdfff).bankr("master_bank");
	map(0xe000, 0xefff).ram().share("share1");
	map(0xf000, 0xf7ff).ram();
	map(0xf800, 0xffff).ram();
}

void djboy_state::mastercpu_port_am(address_map &map)
{
	map.global_mask(0xff);
	map(0x00, 0x00).w(this, FUNC(djboy_state::mastercpu_bankswitch_w));
}

/******************************************************************************/

void djboy_state::slavecpu_am(address_map &map)
{
	map(0x0000, 0x7fff).rom();
	map(0x8000, 0xbfff).bankr("slave_bank");
	map(0xc000, 0xcfff).ram().w(this, FUNC(djboy_state::djboy_videoram_w)).share("videoram");
	map(0xd000, 0xd3ff).ram().w(this, FUNC(djboy_state::djboy_paletteram_w)).share("paletteram");
	map(0xd400, 0xd8ff).ram();
	map(0xe000, 0xffff).ram().share("share1");
}

void djboy_state::slavecpu_port_am(address_map &map)
{
	map.global_mask(0xff);
	map(0x00, 0x00).w(this, FUNC(djboy_state::slavecpu_bankswitch_w));
	map(0x02, 0x02).w(m_soundlatch, FUNC(generic_latch_8_device::write));
	map(0x04, 0x04).r(m_slavelatch, FUNC(generic_latch_8_device::read)).w(m_beastlatch, FUNC(generic_latch_8_device::write));
	map(0x06, 0x06).w(this, FUNC(djboy_state::djboy_scrolly_w));
	map(0x08, 0x08).w(this, FUNC(djboy_state::djboy_scrollx_w));
	map(0x0a, 0x0a).w(this, FUNC(djboy_state::trigger_nmi_on_mastercpu));
	map(0x0c, 0x0c).r(this, FUNC(djboy_state::beast_status_r));
	map(0x0e, 0x0e).w(this, FUNC(djboy_state::coin_count_w));
}

/******************************************************************************/

void djboy_state::soundcpu_am(address_map &map)
{
	map(0x0000, 0x7fff).rom();
	map(0x8000, 0xbfff).bankr("sound_bank");
	map(0xc000, 0xdfff).ram();
}

void djboy_state::soundcpu_port_am(address_map &map)
{
	map.global_mask(0xff);
	map(0x00, 0x00).w(this, FUNC(djboy_state::soundcpu_bankswitch_w));
	map(0x02, 0x03).rw("ymsnd", FUNC(ym2203_device::read), FUNC(ym2203_device::write));
	map(0x04, 0x04).r(m_soundlatch, FUNC(generic_latch_8_device::read));
	map(0x06, 0x06).rw("oki_l", FUNC(okim6295_device::read), FUNC(okim6295_device::write));
	map(0x07, 0x07).rw("oki_r", FUNC(okim6295_device::read), FUNC(okim6295_device::write));
}

/******************************************************************************/

READ8_MEMBER(djboy_state::beast_p0_r)
{
	// ?
	return 0;
}

WRITE8_MEMBER(djboy_state::beast_p0_w)
{
	if (!BIT(m_beast_p0, 1) && BIT(data, 1))
	{
		m_slavelatch->write(space, 0, m_beast_p1);
	}

	if (BIT(data, 0) == 0)
		m_beastlatch->acknowledge_w(space, 0, data);

	m_beast_p0 = data;
}

READ8_MEMBER(djboy_state::beast_p1_r)
{
	if (BIT(m_beast_p0, 0) == 0)
		return m_beastlatch->read(space, 0);
	else
		return 0; // ?
}

WRITE8_MEMBER(djboy_state::beast_p1_w)
{
	m_beast_p1 = data;
}

READ8_MEMBER(djboy_state::beast_p2_r)
{
	switch ((m_beast_p0 >> 2) & 3)
	{
		case 0: return m_port_in[1]->read();
		case 1: return m_port_in[2]->read();
		case 2: return m_port_in[0]->read();
		default: return 0xff;
	}
}

WRITE8_MEMBER(djboy_state::beast_p2_w)
{
	m_beast_p2 = data;
}

READ8_MEMBER(djboy_state::beast_p3_r)
{
	uint8_t dsw = 0;
	uint8_t dsw1 = ~m_port_dsw[0]->read();
	uint8_t dsw2 = ~m_port_dsw[1]->read();

	switch ((m_beast_p0 >> 5) & 3)
	{
		case 0: dsw = (BIT(dsw2, 4) << 3) | (BIT(dsw2, 0) << 2) | (BIT(dsw1, 4) << 1) | BIT(dsw1, 0); break;
		case 1: dsw = (BIT(dsw2, 5) << 3) | (BIT(dsw2, 1) << 2) | (BIT(dsw1, 5) << 1) | BIT(dsw1, 1); break;
		case 2: dsw = (BIT(dsw2, 6) << 3) | (BIT(dsw2, 2) << 2) | (BIT(dsw1, 6) << 1) | BIT(dsw1, 2); break;
		case 3: dsw = (BIT(dsw2, 7) << 3) | (BIT(dsw2, 3) << 2) | (BIT(dsw1, 7) << 1) | BIT(dsw1, 3); break;
	}
	return (dsw << 4) | (m_beastlatch->pending_r() ? 0x0 : 0x4) | (m_slavelatch->pending_r() ? 0x8 : 0x0);
}

WRITE8_MEMBER(djboy_state::beast_p3_w)
{
	m_beast_p3 = data;
	m_slavecpu->set_input_line(INPUT_LINE_RESET, data & 2 ? CLEAR_LINE : ASSERT_LINE);
}
/* Program/data maps are defined in the 8051 core */

/******************************************************************************/

static INPUT_PORTS_START( djboy )
	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* labeled "TEST" in self test */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) /* punch */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) /* kick */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 ) /* jump */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("IN2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("DSW1")
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )      PORT_DIPLOCATION("SW1:1") /* Manual states "CAUTION  !! .... Don't use ." */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Flip_Screen ) )  PORT_DIPLOCATION("SW1:2")
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_SERVICE_DIPLOC(  0x04, IP_ACTIVE_HIGH, "SW1:3" )
//  PORT_DIPNAME( 0x04, 0x00, DEF_STR( Service_Mode ) ) PORT_DIPLOCATION("SW1:3")
//  PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
//  PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )      PORT_DIPLOCATION("SW1:4")
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Coin_A ) )       PORT_DIPLOCATION("SW1:5,6")
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coin_B ) )       PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )

	PORT_START("DSW2")
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Difficulty ) )   PORT_DIPLOCATION("SW2:1,2")
	PORT_DIPSETTING(    0x01, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x0c, 0x00, "Bonus Levels (in thousands)" ) PORT_DIPLOCATION("SW2:3,4")
	PORT_DIPSETTING(    0x00, "10,30,50,70,90" )
	PORT_DIPSETTING(    0x04, "10,20,30,40,50,60,70,80,90" )
	PORT_DIPSETTING(    0x08, "20,50" )
	PORT_DIPSETTING(    0x0c, DEF_STR( None ) )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )        PORT_DIPLOCATION("SW2:5,6")
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPSETTING(    0x20, "7" )
	PORT_DIPSETTING(    0x30, "9" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )  PORT_DIPLOCATION("SW2:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Stereo Sound" )      PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


static const gfx_layout tile_layout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ STEP8(0,4), STEP8(4*8*8,4) },
	{ STEP8(0,4*8), STEP8(4*8*8*2,4*8) },
	16*16*4
};

static GFXDECODE_START( djboy )
	GFXDECODE_ENTRY( "gfx1", 0, tile_layout, 0x100, 16 ) /* sprite bank */
	GFXDECODE_ENTRY( "gfx2", 0, tile_layout, 0x000, 16 ) /* background tiles */
GFXDECODE_END

/******************************************************************************/

/* Main Z80 uses IM2 */
TIMER_DEVICE_CALLBACK_MEMBER(djboy_state::djboy_scanline)
{
	int scanline = param;

	if(scanline == 240) // vblank-out irq
		m_mastercpu->set_input_line_and_vector(0, HOLD_LINE, 0xfd);

	/* Pandora "sprite end dma" irq? TODO: timing is clearly off, attract mode relies on this */
	if(scanline == 64)
		m_mastercpu->set_input_line_and_vector(0, HOLD_LINE, 0xff);
}

void djboy_state::machine_start()
{
	uint8_t *MASTER = memregion("mastercpu")->base();
	uint8_t *SLAVE = memregion("slavecpu")->base();
	uint8_t *SOUND = memregion("soundcpu")->base();

	m_masterbank->configure_entries(0, 32, &MASTER[0x00000], 0x2000);
	m_slavebank->configure_entries(0, 4, &SLAVE[0x00000], 0x4000);
	m_slavebank->configure_entries(8, 8, &SLAVE[0x10000], 0x4000);
	m_soundbank->configure_entries(0, 8, &SOUND[0x00000], 0x4000);
	m_masterbank_l->configure_entry(0, &MASTER[0x08000]); /* unsure if/how this area is banked */

	save_item(NAME(m_videoreg));
	save_item(NAME(m_scrollx));
	save_item(NAME(m_scrolly));

	/* Kaneko BEAST */
	save_item(NAME(m_beast_p0));
	save_item(NAME(m_beast_p1));
	save_item(NAME(m_beast_p2));
	save_item(NAME(m_beast_p3));
}

void djboy_state::machine_reset()
{
	m_videoreg = 0;
	m_scrollx = 0;
	m_scrolly = 0;
}

MACHINE_CONFIG_START(djboy_state::djboy)

	MCFG_CPU_ADD("mastercpu", Z80, 6000000)
	MCFG_CPU_PROGRAM_MAP(mastercpu_am)
	MCFG_CPU_IO_MAP(mastercpu_port_am)
	MCFG_TIMER_DRIVER_ADD_SCANLINE("scantimer", djboy_state, djboy_scanline, "screen", 0, 1)

	MCFG_CPU_ADD("slavecpu", Z80, 6000000)
	MCFG_CPU_PROGRAM_MAP(slavecpu_am)
	MCFG_CPU_IO_MAP(slavecpu_port_am)
	MCFG_CPU_VBLANK_INT_DRIVER("screen", djboy_state,  irq0_line_hold)

	MCFG_CPU_ADD("soundcpu", Z80, 6000000)
	MCFG_CPU_PROGRAM_MAP(soundcpu_am)
	MCFG_CPU_IO_MAP(soundcpu_port_am)
	MCFG_CPU_VBLANK_INT_DRIVER("screen", djboy_state,  irq0_line_hold)

	MCFG_CPU_ADD("beast", I80C51, 6000000)
	MCFG_MCS51_PORT_P0_IN_CB(READ8(djboy_state, beast_p0_r))
	MCFG_MCS51_PORT_P0_OUT_CB(WRITE8(djboy_state, beast_p0_w))
	MCFG_MCS51_PORT_P1_IN_CB(READ8(djboy_state, beast_p1_r))
	MCFG_MCS51_PORT_P1_OUT_CB(WRITE8(djboy_state, beast_p1_w))
	MCFG_MCS51_PORT_P2_IN_CB(READ8(djboy_state, beast_p2_r))
	MCFG_MCS51_PORT_P2_OUT_CB(WRITE8(djboy_state, beast_p2_w))
	MCFG_MCS51_PORT_P3_IN_CB(READ8(djboy_state, beast_p3_r))
	MCFG_MCS51_PORT_P3_OUT_CB(WRITE8(djboy_state, beast_p3_w))

	MCFG_QUANTUM_TIME(attotime::from_hz(6000))
	
	MCFG_GENERIC_LATCH_8_ADD("slavelatch")

	MCFG_GENERIC_LATCH_8_ADD("beastlatch")
	MCFG_GENERIC_LATCH_DATA_PENDING_CB(INPUTLINE("beast", INPUT_LINE_IRQ0))
	MCFG_GENERIC_LATCH_SEPARATE_ACKNOWLEDGE(true)

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(57.5)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(256, 256)
	MCFG_SCREEN_VISIBLE_AREA(0, 256-1, 16, 256-16-1)
	MCFG_SCREEN_UPDATE_DRIVER(djboy_state, screen_update_djboy)
	MCFG_SCREEN_VBLANK_CALLBACK(WRITELINE(djboy_state, screen_vblank_djboy))
	MCFG_SCREEN_PALETTE("palette")

	MCFG_GFXDECODE_ADD("gfxdecode", "palette", djboy)
	MCFG_PALETTE_ADD("palette", 0x200)

	MCFG_DEVICE_ADD("pandora", KANEKO_PANDORA, 0)
	MCFG_KANEKO_PANDORA_GFXDECODE("gfxdecode")

	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MCFG_GENERIC_LATCH_8_ADD("soundlatch")
	MCFG_GENERIC_LATCH_DATA_PENDING_CB(INPUTLINE("soundcpu", INPUT_LINE_NMI))

	MCFG_SOUND_ADD("ymsnd", YM2203, 3000000)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 0.40)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 0.40)

	MCFG_OKIM6295_ADD("oki_l", 12000000 / 8, PIN7_LOW)
	MCFG_DEVICE_ROM("oki")
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 0.50)

	MCFG_OKIM6295_ADD("oki_r", 12000000 / 8, PIN7_LOW)
	MCFG_DEVICE_ROM("oki")
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 0.50)
MACHINE_CONFIG_END


ROM_START( djboy )
	ROM_REGION( 0x40000, "mastercpu", 0 )
	ROM_LOAD( "bs64.4b",   0x00000, 0x20000, CRC(b77aacc7) SHA1(78100d4695738a702f13807526eb1bcac759cce3) )
	ROM_LOAD( "bs100.4d",  0x20000, 0x20000, CRC(081e8af8) SHA1(3589dab1cf31b109a40370b4db1f31785023e2ed) )

	ROM_REGION( 0x30000, "slavecpu", 0 )
	ROM_LOAD( "bs65.5y",   0x00000, 0x10000, CRC(0f1456eb) SHA1(62ed48c0d71c1fabbb3f6ada60381f57f692cef8) )
	ROM_LOAD( "bs101.6w",  0x10000, 0x20000, CRC(a7c85577) SHA1(8296b96d5f69f6c730b7ed77fa8c93496b33529c) )

	ROM_REGION( 0x20000, "soundcpu", 0 ) /* sound */
	ROM_LOAD( "bs200.8c",  0x00000, 0x20000, CRC(f6c19e51) SHA1(82193f71122df07cce0a7f057a87b89eb2d587a1) )

	ROM_REGION( 0x1000, "beast", 0 ) /* MSM80C51F microcontroller */
	ROM_LOAD( "beast.9s", 0x00000, 0x1000, CRC(ebe0f5f3) SHA1(6081343c9b4510c4c16b71f6340266a1f76170ac) ) /* Internal ROM image */

	ROM_REGION( 0x200000, "gfx1", 0 ) /* sprites */
	ROM_LOAD( "bs000.1h", 0x000000, 0x80000, CRC(be4bf805) SHA1(a73c564575fe89d26225ca8ec2d98b6ac319ac18) )
	ROM_LOAD( "bs001.1f", 0x080000, 0x80000, CRC(fdf36e6b) SHA1(a8762458dfd5201304247c113ceb85e96e33d423) )
	ROM_LOAD( "bs002.1d", 0x100000, 0x80000, CRC(c52fee7f) SHA1(bd33117f7a57899fd4ec0a77413107edd9c44629) )
	ROM_LOAD( "bs003.1k", 0x180000, 0x80000, CRC(ed89acb4) SHA1(611af362606b73cd2cf501678b463db52dcf69c4) )
	ROM_LOAD( "bs07.1b",  0x1f0000, 0x10000, CRC(d9b7a220) SHA1(ba3b528d50650c209c986268bb29b42ff1276eb2) )  // replaces last 0x200 tiles

	ROM_REGION( 0x100000, "gfx2", 0 ) /* background */
	ROM_LOAD( "bs004.1s", 0x000000, 0x80000, CRC(2f1392c3) SHA1(1bc3030b3612766a02133eef0b4d20013c0495a4) )
	ROM_LOAD( "bs005.1u", 0x080000, 0x80000, CRC(46b400c4) SHA1(35f4823364bbff1fc935994498d462bbd3bc6044) )

	ROM_REGION( 0x40000, "oki", 0 ) /* OKI-M6295 samples */
	ROM_LOAD( "bs203.5j", 0x000000, 0x40000, CRC(805341fb) SHA1(fb94e400e2283aaa806814d5a39d6196457dc822) )
ROM_END

ROM_START( djboya )
	ROM_REGION( 0x40000, "mastercpu", 0 )
	ROM_LOAD( "bs19s.rom", 0x00000, 0x20000, CRC(17ce9f6c) SHA1(a0c1832b05dc46991e8949067ca0278f5498835f) )
	ROM_LOAD( "bs100.4d",  0x20000, 0x20000, CRC(081e8af8) SHA1(3589dab1cf31b109a40370b4db1f31785023e2ed) )

	ROM_REGION( 0x30000, "slavecpu", 0 )
	ROM_LOAD( "bs15s.rom", 0x00000, 0x10000, CRC(e6f966b2) SHA1(f9df16035a8b09d87eb70315b216892e25d99b03) )
	ROM_LOAD( "bs101.6w",  0x10000, 0x20000, CRC(a7c85577) SHA1(8296b96d5f69f6c730b7ed77fa8c93496b33529c) )

	ROM_REGION( 0x20000, "soundcpu", 0 ) /* sound */
	ROM_LOAD( "bs200.8c",  0x00000, 0x20000, CRC(f6c19e51) SHA1(82193f71122df07cce0a7f057a87b89eb2d587a1) )

	ROM_REGION( 0x1000, "beast", 0 ) /* MSM80C51F microcontroller */
	ROM_LOAD( "beast.9s", 0x00000, 0x1000, CRC(ebe0f5f3) SHA1(6081343c9b4510c4c16b71f6340266a1f76170ac) ) /* Internal ROM image */

	ROM_REGION( 0x200000, "gfx1", 0 ) /* sprites */
	ROM_LOAD( "bs000.1h", 0x000000, 0x80000, CRC(be4bf805) SHA1(a73c564575fe89d26225ca8ec2d98b6ac319ac18) )
	ROM_LOAD( "bs001.1f", 0x080000, 0x80000, CRC(fdf36e6b) SHA1(a8762458dfd5201304247c113ceb85e96e33d423) )
	ROM_LOAD( "bs002.1d", 0x100000, 0x80000, CRC(c52fee7f) SHA1(bd33117f7a57899fd4ec0a77413107edd9c44629) )
	ROM_LOAD( "bs003.1k", 0x180000, 0x80000, CRC(ed89acb4) SHA1(611af362606b73cd2cf501678b463db52dcf69c4) )
	ROM_LOAD( "bs07.1b",  0x1f0000, 0x10000, CRC(d9b7a220) SHA1(ba3b528d50650c209c986268bb29b42ff1276eb2) )  // replaces last 0x200 tiles

	ROM_REGION( 0x100000, "gfx2", 0 ) /* background */
	ROM_LOAD( "bs004.1s", 0x000000, 0x80000, CRC(2f1392c3) SHA1(1bc3030b3612766a02133eef0b4d20013c0495a4) )
	ROM_LOAD( "bs005.1u", 0x080000, 0x80000, CRC(46b400c4) SHA1(35f4823364bbff1fc935994498d462bbd3bc6044) )

	ROM_REGION( 0x40000, "oki", 0 ) /* OKI-M6295 samples */
	ROM_LOAD( "bs203.5j", 0x000000, 0x40000, CRC(805341fb) SHA1(fb94e400e2283aaa806814d5a39d6196457dc822) )
ROM_END

ROM_START( djboyj )
	ROM_REGION( 0x40000, "mastercpu", 0 )
	ROM_LOAD( "bs12.4b",   0x00000, 0x20000, CRC(0971523e) SHA1(f90cd02cedf8632f4b651de7ea75dc8c0e682f6e) )
	ROM_LOAD( "bs100.4d",  0x20000, 0x20000, CRC(081e8af8) SHA1(3589dab1cf31b109a40370b4db1f31785023e2ed) )

	ROM_REGION( 0x30000, "slavecpu", 0 )
	ROM_LOAD( "bs13.5y",   0x00000, 0x10000, CRC(5c3f2f96) SHA1(bb7ee028a2d8d3c76a78a29fba60bcc36e9399f5) )
	ROM_LOAD( "bs101.6w",  0x10000, 0x20000, CRC(a7c85577) SHA1(8296b96d5f69f6c730b7ed77fa8c93496b33529c) )

	ROM_REGION( 0x20000, "soundcpu", 0 ) /* sound */
	ROM_LOAD( "bs200.8c",  0x00000, 0x20000, CRC(f6c19e51) SHA1(82193f71122df07cce0a7f057a87b89eb2d587a1) )

	ROM_REGION( 0x1000, "beast", 0 ) /* MSM80C51F microcontroller */
	ROM_LOAD( "beast.9s", 0x00000, 0x1000, CRC(ebe0f5f3) SHA1(6081343c9b4510c4c16b71f6340266a1f76170ac) ) /* Internal ROM image */

	ROM_REGION( 0x200000, "gfx1", 0 ) /* sprites */
	ROM_LOAD( "bs000.1h", 0x000000, 0x80000, CRC(be4bf805) SHA1(a73c564575fe89d26225ca8ec2d98b6ac319ac18) )
	ROM_LOAD( "bs001.1f", 0x080000, 0x80000, CRC(fdf36e6b) SHA1(a8762458dfd5201304247c113ceb85e96e33d423) )
	ROM_LOAD( "bs002.1d", 0x100000, 0x80000, CRC(c52fee7f) SHA1(bd33117f7a57899fd4ec0a77413107edd9c44629) )
	ROM_LOAD( "bs003.1k", 0x180000, 0x80000, CRC(ed89acb4) SHA1(611af362606b73cd2cf501678b463db52dcf69c4) )
	ROM_LOAD( "bsxx.1b",  0x1f0000, 0x10000, CRC(22c8aa08) SHA1(5521c9d73b4ee82a2de1992d6edc7ef62788ad72) ) // replaces last 0x200 tiles

	ROM_REGION( 0x100000, "gfx2", 0 ) /* background */
	ROM_LOAD( "bs004.1s", 0x000000, 0x80000, CRC(2f1392c3) SHA1(1bc3030b3612766a02133eef0b4d20013c0495a4) )
	ROM_LOAD( "bs005.1u", 0x080000, 0x80000, CRC(46b400c4) SHA1(35f4823364bbff1fc935994498d462bbd3bc6044) )

	ROM_REGION( 0x40000, "oki", 0 ) /* OKI-M6295 samples */
	ROM_LOAD( "bs-204.5j", 0x000000, 0x40000, CRC(510244f0) SHA1(afb502d46d268ad9cd209ae1da72c50e4e785626) )
ROM_END


DRIVER_INIT_MEMBER(djboy_state,djboy)
{
	m_bankxor = 0x00;
}

DRIVER_INIT_MEMBER(djboy_state,djboyj)
{
	m_bankxor = 0x1f;
}

/*     YEAR, NAME,  PARENT, MACHINE, INPUT, INIT, MNTR,  COMPANY, FULLNAME, FLAGS */
GAME( 1989, djboy,  0,      djboy,   djboy, djboy_state, djboy,    ROT0, "Kaneko (American Sammy license)", "DJ Boy (set 1)", MACHINE_SUPPORTS_SAVE) // Sammy & Williams logos in FG ROM
GAME( 1989, djboya, djboy,  djboy,   djboy, djboy_state, djboy,    ROT0, "Kaneko (American Sammy license)", "DJ Boy (set 2)", MACHINE_SUPPORTS_SAVE) // Sammy & Williams logos in FG ROM
GAME( 1989, djboyj, djboy,  djboy,   djboy, djboy_state, djboyj,   ROT0, "Kaneko (Sega license)", "DJ Boy (Japan)", MACHINE_SUPPORTS_SAVE ) // Sega logo in FG ROM
