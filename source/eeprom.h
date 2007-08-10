/* this defines the type of EEPROM inside the game cartridge as Backup RAM
 * Here are some notes from 8BitWizard (http://www.spritesmind.net/_GenDev/forum):
 *
 * Mode 1 (7-bit) - the chip takes a single byte with a 7-bit memory address and a R/W bit (24C01)
 * Mode 2 (8-bit) - the chip takes a 7-bit device address and R/W bit followed by an 8-bit memory address;
 * the device address may contain up to three more memory address bits (24C01 - 24C16).
 * You can also string eight 24C01, four 24C02, two 24C08, or various combinations, set their address config lines correctly,
 * and the result appears exactly the same as a 24C16
 * Mode 3 (16-bit) - the chip takes a 7-bit device address and R/W bit followed by a 16-bit memory address (24C32 and larger)
 *
 * Also, while most 24Cxx are addressed at 200000-2FFFFF, I have found two different ways of mapping the control lines. 
 * EA uses SDA on D7 (read/write) and SCL on D6 (write only), and I have found boards using different mapping (I think Accolade)
 * which uses D1-read=SDA, D0-write=SDA, D1-write=SCL. Accolade also has a custom-chip mapper which may even use a third method. 
 */

typedef struct
{
	uint8 address_bits;		/* number of bits needed to address the array: 7, 8 or 16 */
	uint16 size_mask;		/* size of the array (in bytes) - 1 */
	uint16 pagewrite_mask;	/* maximal number of bytes that can be written in a single write cycle - 1*/
	uint32 sda_in_adr;		/* 68k address used by SDA_IN signal */
	uint32 sda_out_adr;		/* 68k address used by SDA_OUT signal  */
	uint32 scl_adr;			/* address used by SCL signal  */
	uint8 sda_in_bit;		/* position of the SDA_IN bit */
	uint8 sda_out_bit;		/* position of the SDA_OUT bit */
	uint8 scl_bit;			/* position of the SCL bit */

} T_EEPROM_TYPE;

typedef enum
{
	STAND_BY = 0,
	WAIT_STOP,
	GET_SLAVE_ADR,
	GET_WORD_ADR_7BITS,
	GET_WORD_ADR_HIGH,
	GET_WORD_ADR_LOW,
	WRITE_DATA,
	READ_DATA,

} T_EEPROM_STATE;

typedef struct
{
	uint8 sda;
	uint8 scl;
	uint8 old_sda;
	uint8 old_scl;
	uint8 cycles;
	uint8 rw;
	uint16 slave_mask;
	uint16 word_address;
	T_EEPROM_STATE state;
	T_EEPROM_TYPE type;
} T_EEPROM;

/* global variables */
extern T_EEPROM eeprom;

/* Function prototypes */
extern void EEPROM_Init();
extern void EEPROM_Write(unsigned int address, unsigned int value);
extern unsigned int EEPROM_Read(unsigned int address);

