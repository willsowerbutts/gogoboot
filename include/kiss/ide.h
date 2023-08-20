#ifndef __GOGOBOOT_KISS_IDE_DOT_H__
#define __GOGOBOOT_KISS_IDE_DOT_H__

/* idexfer.s */
void ide_sector_xfer_input(void *buf, volatile uint8_t *port);
void ide_sector_xfer_output(const void *buf, volatile uint8_t *port);

struct disk_controller_t
{
    uint16_t base_io;
    volatile uint8_t *lsb;
    volatile uint8_t *msb;
    volatile uint8_t *select;
    volatile uint8_t *control;
    bool read_mode;
};

/* IDE control signal to 8255 port C mapping */
#define PPIDE_A0_LINE           0x01 // Direct from 8255 to IDE interface
#define PPIDE_A1_LINE           0x02 // Direct from 8255 to IDE interface
#define PPIDE_A2_LINE           0x04 // Direct from 8255 to IDE interface
#define PPIDE_CS0_LINE          0x08 // Inverter between 8255 and IDE interface
#define PPIDE_CS1_LINE          0x10 // Inverter between 8255 and IDE interface
#define PPIDE_WR_LINE           0x20 // Inverter between 8255 and IDE interface
#define PPIDE_RD_LINE           0x40 // Inverter between 8255 and IDE interface
#define PPIDE_RST_LINE          0x80 // Inverter between 8255 and IDE interface
#define PPIDE_WR_BIT            5    // (1 << PPIDE_WR_BIT) = PPIDE_WR_LINE
#define PPIDE_RD_BIT            6    // (1 << PPIDE_RD_BIT) = PPIDE_RD_LINE

/* 8255 configuration */
#define PPIDE_PPI_BUS_READ      0x92
#define PPIDE_PPI_BUS_WRITE     0x80

/*            Will's amazing PPIDE register cheat sheet
/CS0 /CS1 A2 A1 A0  Addr.   Read Function       Write Function        Linux name               PPIDE name
-----------------------------------------------------------------------------------------------------------------------
  0    1   0  0  0   1F0    Data Register       Data Register         data_addr                PPIDE_REG_DATA
  0    1   0  0  1   1F1    Error Register      (Write Precomp Reg.)  error_addr/feature_addr  PPIDE_REG_ERROR/FEATURES
  0    1   0  1  0   1F2    Sector Count        Sector Count          nsect_addr               PPIDE_REG_SEC_COUNT
  0    1   0  1  1   1F3    Sector Number       Sector Number         lbal_addr                PPIDE_REG_LBA_0
  0    1   1  0  0   1F4    Cylinder Low        Cylinder Low          lbam_addr                PPIDE_REG_LBA_1
  0    1   1  0  1   1F5    Cylinder High       Cylinder High         lbah_addr                PPIDE_REG_LBA_2
  0    1   1  1  0   1F6    SDH Register        SDH Register          device_addr              PPIDE_REG_DEVHEAD
  0    1   1  1  1   1F7    Status Register     Command Register      status_addr              PPIDE_REG_STATUS
  1    0   1  1  0   3F6    Alternate Status    Digital Output        ctl_addr                 PPIDE_REG_ALTSTATUS
  1    0   1  1  1   3F7    Drive Address       Not Used              -                        -
*/

#define PPIDE_REG_DATA      (PPIDE_CS0_LINE)
#define PPIDE_REG_ERROR     (PPIDE_CS0_LINE | PPIDE_A0_LINE)
#define PPIDE_REG_FEATURES  (PPIDE_CS0_LINE | PPIDE_A0_LINE)
#define PPIDE_REG_SEC_COUNT (PPIDE_CS0_LINE | PPIDE_A1_LINE)
#define PPIDE_REG_LBA_0     (PPIDE_CS0_LINE | PPIDE_A1_LINE | PPIDE_A0_LINE)
#define PPIDE_REG_LBA_1     (PPIDE_CS0_LINE | PPIDE_A2_LINE)
#define PPIDE_REG_LBA_2     (PPIDE_CS0_LINE | PPIDE_A2_LINE | PPIDE_A0_LINE)
#define PPIDE_REG_LBA_3     (PPIDE_CS0_LINE | PPIDE_A2_LINE | PPIDE_A1_LINE)
#define PPIDE_REG_DEVHEAD   (PPIDE_CS0_LINE | PPIDE_A2_LINE | PPIDE_A1_LINE)
#define PPIDE_REG_COMMAND   (PPIDE_CS0_LINE | PPIDE_A2_LINE | PPIDE_A1_LINE | PPIDE_A0_LINE)
#define PPIDE_REG_STATUS    (PPIDE_CS0_LINE | PPIDE_A2_LINE | PPIDE_A1_LINE | PPIDE_A0_LINE)
#define PPIDE_REG_ALTSTATUS (PPIDE_CS1_LINE | PPIDE_A2_LINE | PPIDE_A1_LINE)

#endif
