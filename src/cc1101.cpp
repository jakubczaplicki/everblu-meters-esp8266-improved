/*  the radian_trx SW shall not be distributed  nor used for commercial product*/
/*  it is exposed just to demonstrate CC1101 capability to reader water meter indexes */
/*  there is no Warranty on radian_trx SW */

#include "private.h"          // Secrets + GDO0, METER_*, FREQUENCY - copy from Example_Private.h
#include "config.h"           // SPI pins (non-secret hardware config)
#include "everblu_meters.h"
#include "utils.h"
#include "cc1101.h"
#include "radian_constants.h"
#include <Arduino.h>
#include <SPI.h>

uint8_t RF_config_u8 = 0xFF;
uint8_t PA[] = { 0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00, };
uint8_t CC1101_status_state = 0;
uint8_t CC1101_status_FIFO_FreeByte = 0;
uint8_t CC1101_status_FIFO_ReadByte = 0;
uint8_t debug_out = 0;

#ifndef TRUE
#define TRUE true
#endif

#ifndef FALSE
#define FALSE false
#endif

#define TX_LOOP_OUT 300
/*---------------------------[CC1100 - R/W offsets]------------------------------*/
#define WRITE_SINGLE_BYTE  		0x00
#define WRITE_BURST  			0x40
#define READ_SINGLE_BYTE  		0x80
#define READ_BURST  			0xC0

/*-------------------------[CC1100 - config register]----------------------------*/
#define IOCFG2  			0x00                                    // GDO2 output pin configuration
#define IOCFG1  			0x01                                    // GDO1 output pin configuration
#define IOCFG0  			0x02                                    // GDO0 output pin configuration
#define FIFOTHR  			0x03                                    // RX FIFO and TX FIFO thresholds
#define SYNC1  			    0x04                                    // Sync word, high byte
#define SYNC0  			    0x05                                    // Sync word, low byte
#define PKTLEN  			0x06                                    // Packet length
#define PKTCTRL1 			0x07                                  	// Packet automation control
#define PKTCTRL0  			0x08                                  	// Packet automation control
#define ADDRR  				0x09                                    // Device address
#define CHANNR  			0x0A                                    // Channel number
#define FSCTRL1  			0x0B                                   	// Frequency synthesizer control
#define FSCTRL0  			0x0C                                   	// Frequency synthesizer control
#define FREQ2  			    0x0D                                    // Frequency control word, high byte
#define FREQ1  			    0x0E                                    // Frequency control word, middle byte
#define FREQ0  			    0x0F                                    // Frequency control word, low byte

#define MDMCFG4  			0x10                                   	// Modem configuration
#define MDMCFG3  			0x11                                   	// Modem configuration
#define MDMCFG2  			0x12                                   	// Modem configuration
#define MDMCFG1  			0x13                                   	// Modem configuration
#define MDMCFG0  			0x14                                   	// Modem configuration
#define DEVIATN  			0x15                                   	// Modem deviation setting
#define MCSM2  			0x16                                    // Main Radio Cntrl State Machine config
#define MCSM1  			0x17                                    // Main Radio Cntrl State Machine config
#define MCSM0  			0x18                                    // Main Radio Cntrl State Machine config
#define FOCCFG  			0x19	                                // Frequency Offset Compensation config
#define BSCFG  			0x1A                                    // Bit Synchronization configuration
#define AGCCTRL2 			0x1B                                    // AGC control
#define AGCCTRL1 			0x1C                                    // AGC control
#define AGCCTRL0 			0x1D                                    // AGC control
#define WOREVT1 			0x1E                                   	// High byte Event 0 timeout
#define WOREVT0 			0x1F                                   	// Low byte Event 0 timeout

#define WORCTRL 			0x20                                   	// Wake On Radio control
#define FREND1 			0x21                                    // Front end RX configuration
#define FREND0 			0x22                                    // Front end TX configuration
#define FSCAL3 			0x23                                    // Frequency synthesizer calibration
#define FSCAL2 			0x24                                    // Frequency synthesizer calibration
#define FSCAL1 			0x25                                    // Frequency synthesizer calibration
#define FSCAL0 			0x26                                    // Frequency synthesizer calibration
#define RCCTRL1 			0x27                                   	// RC oscillator configuration
#define RCCTRL0 			0x28                                   	// RC oscillator configuration
#define FSTEST 			0x29                                   	// Frequency synthesizer cal control
#define PTEST 				0x2A                                    // Production test
#define AGCTEST 			0x2B                                   	// AGC test
#define TEST2 				0x2C                                    // Various test settings
#define TEST1 				0x2D                                    // Various test settings
#define TEST0 				0x2E                                    // Various test settings

int _spi_speed = 0;
int wiringPiSPIDataRW(int channel, unsigned char *data, int len)
{
  (void)channel;
  if (!_spi_speed) return -1;

  SPI.beginTransaction(SPISettings(_spi_speed, MSBFIRST, SPI_MODE0));
  digitalWrite(SPI_SS, 0);

  //echo_debug(debug_out, "wiringPiSPIDataRW(0x%02X, %d)\n", (len > 0) ? data[0] : 'X' , len);

  SPI.transfer(data, len);

  digitalWrite(SPI_SS, 1);
  SPI.endTransaction();

  return 0;
}

int wiringPiSPISetup(int channel, int speed)
{
  (void)channel;
  _spi_speed = speed;

  pinMode(SPI_SS, OUTPUT);
  digitalWrite(SPI_SS, 1);

#ifdef ESP8266
  SPI.pins(SPI_CSK, SPI_MISO, SPI_MOSI, SPI_SS);
  SPI.begin();
#endif

#ifdef ESP32
  SPI.begin(SPI_CSK, SPI_MISO, SPI_MOSI, SPI_SS);
#endif

  return 0;
}

/*----------------------------[END config register]------------------------------*/
//------------------[write register]--------------------------------
uint8_t halRfWriteReg(uint8_t reg_addr, uint8_t value)
{
  uint8_t tbuf[2] = { 0 };
  tbuf[0] = reg_addr | WRITE_SINGLE_BYTE;
  tbuf[1] = value;
  uint8_t len = 2;
  wiringPiSPIDataRW(0, tbuf, len);
  CC1101_status_FIFO_FreeByte = tbuf[1] & 0x0F;
  CC1101_status_state = (tbuf[0] >> 4) & 0x0F;

  return TRUE;
}

/*-------------------------[CC1100 - status register]----------------------------*/
/* 0x3? is replace by 0xF? because for status register burst bit shall be set */
#define PARTNUM_ADDR 			0xF0				// Part number
#define VERSION_ADDR 			0xF1				// Current version number
#define FREQEST_ADDR 			0xF2				// Frequency offset estimate
#define LQI_ADDR 				0xF3				// Demodulator estimate for link quality
#define RSSI_ADDR 				0xF4				// Received signal strength indication
#define MARCSTATE_ADDR 			0xF5				// Control state machine state
#define WORTIME1_ADDR 			0xF6				// High byte of WOR timer
#define WORTIME0_ADDR 			0xF7				// Low byte of WOR timer
#define PKTSTATUS_ADDR 			0xF8				// Current GDOx status and packet status
#define VCO_VC_DAC_ADDR 		0xF9				// Current setting from PLL cal module
#define TXBYTES_ADDR 			0xFA				// Underflow and # of bytes in TXFIFO
#define RXBYTES_ADDR 			0xFB				// Overflow and # of bytes in RXFIFO
//----------------------------[END status register]-------------------------------
#define RXBYTES_MASK            0x7F        // Mask "# of bytes" field in _RXBYTES

uint8_t halRfReadReg(uint8_t spi_instr)
{
  uint8_t value;
  uint8_t rbuf[2] = { 0 };
  uint8_t len = 2;

  //rbuf[0] = spi_instr | READ_SINGLE_BYTE;
  //rbuf[1] = 0;
  //wiringPiSPIDataRW (0, rbuf, len) ;
  //errata Section 3. You have to make sure that you read the same value of the register twice in a row before you evaluate it otherwise you might read a value that is a mix of 2 state values.
  rbuf[0] = spi_instr | READ_SINGLE_BYTE;
  rbuf[1] = 0;
  wiringPiSPIDataRW(0, rbuf, len);
  CC1101_status_FIFO_ReadByte = rbuf[0] & 0x0F;
  CC1101_status_state = (rbuf[0] >> 4) & 0x0F;
  value = rbuf[1];
  return value;
}

#define PATABLE_ADDR  			0x3E                                    // Pa Table Adress
#define TX_FIFO_ADDR		 	0x3F                            		
#define RX_FIFO_ADDR		 	0xBF                            		
void SPIReadBurstReg(uint8_t spi_instr, uint8_t *pArr, uint8_t len)
{
  uint8_t rbuf[len + 1];
  uint8_t i = 0;
  memset(rbuf, 0, len + 1);
  rbuf[0] = spi_instr | READ_BURST;
  wiringPiSPIDataRW(0, rbuf, len + 1);
  for (i = 0; i < len; i++)
  {
    pArr[i] = rbuf[i + 1];
    //echo_debug(debug_out,"SPI_arr_read: 0x%02X\n", pArr[i]);
  }
  CC1101_status_FIFO_ReadByte = rbuf[0] & 0x0F;
  CC1101_status_state = (rbuf[0] >> 4) & 0x0F;
}

void SPIWriteBurstReg(uint8_t spi_instr, uint8_t *pArr, uint8_t len)
{
  uint8_t tbuf[len + 1];
  uint8_t i = 0;
  tbuf[0] = spi_instr | WRITE_BURST;
  for (i = 0; i < len; i++)
  {
    tbuf[i + 1] = pArr[i];
    //echo_debug(debug_out,"SPI_arr_write: 0x%02X\n", tbuf[i+1]);
  }
  wiringPiSPIDataRW(0, tbuf, len + 1);
  CC1101_status_FIFO_FreeByte = tbuf[len] & 0x0F;
  CC1101_status_state = (tbuf[len] >> 4) & 0x0F;
}

/*---------------------------[CC1100-command strobes]----------------------------*/
#define SRES  					0x30                                    // Reset chip
#define SFSTXON  				0x31                                    // Enable/calibrate freq synthesizer
#define SXOFF  					0x32                                    // Turn off crystal oscillator.
#define SCAL 					0x33                                    // Calibrate freq synthesizer & disable
#define SRX  					0x34                                    // Enable RX.
#define STX  					0x35                                    // Enable TX.
#define SIDLE  					0x36                                    // Exit RX / TX
#define SAFC  					0x37                                    // AFC adjustment of freq synthesizer
#define SWOR  					0x38                                    // Start automatic RX polling sequence
#define SPWD  					0x39                                    // Enter pwr down mode when CSn goes hi
#define SFRX  					0x3A                                    // Flush the RX FIFO buffer.
#define SFTX  					0x3B                                    // Flush the TX FIFO buffer.
#define SWORRST  				0x3C                                    // Reset real time clock.
#define SNOP  					0x3D                                    // No operation.
/*----------------------------[END command strobes]------------------------------*/
void CC1101_CMD(uint8_t spi_instr)
{
  uint8_t tbuf[1] = { 0 };
  tbuf[0] = spi_instr | WRITE_SINGLE_BYTE;
  //echo_debug(debug_out,"SPI_data: 0x%02X\n", tbuf[0]);
  wiringPiSPIDataRW(0, tbuf, 1);
  CC1101_status_state = (tbuf[0] >> 4) & 0x0F;
}

void echo_cc1101_version(void);
void show_cc1101_registers_settings(void);

//---------------[CC1100 reset functions "200us"]-----------------------
void cc1101_reset(void)
{
  CC1101_CMD(SRES);
  delay(1); // 1ms for chip reset

  CC1101_CMD(SFTX);   //flush the TX_fifo content -> a must for interrupt handling
  CC1101_CMD(SFRX);	//flush the RX_fifo content -> a must for interrupt handling	
}

void setMHZ(float mhz) {
  int freq2 = 0;
  int freq1 = 0;
  int freq0 = 0;

  for (bool i = 0; i == 0;) {
    if (mhz >= 26) {
      mhz -= 26;
      freq2 += 1;
    }
    else if (mhz >= 0.1015625) {
      mhz -= 0.1015625;
      freq1 += 1;
    }
    else if (mhz >= 0.00039675) {
      mhz -= 0.00039675;
      freq0 += 1;
    }
    else { i = 1; }
  }
  if (freq0 > 255) { freq1 += 1; freq0 -= 256; }
  halRfWriteReg(FREQ2, (uint8_t)freq2);
  halRfWriteReg(FREQ1, (uint8_t)freq1);
  halRfWriteReg(FREQ0, (uint8_t)freq0);
}

void cc1101_configureRF_0(float freq)
{
  RF_config_u8 = 0;
  //
  // Rf settings for CC1101
  //
  halRfWriteReg(IOCFG2, 0x0D);  //GDO2 Output Pin Configuration : Serial Data Output
  halRfWriteReg(IOCFG0, 0x06);  //GDO0 Output Pin Configuration : Asserts when sync word has been sent / received, and de-asserts at the end of the packet.
  halRfWriteReg(FIFOTHR, 0x47); //0x4? adc with bandwith< 325khz
  halRfWriteReg(SYNC1, 0x55);   //01010101
  halRfWriteReg(SYNC0, 0x00);
  halRfWriteReg(PKTCTRL1, 0x00);
  halRfWriteReg(PKTCTRL0, 0x00);
  halRfWriteReg(FSCTRL1, 0x08);
  setMHZ(freq);
  halRfWriteReg(MDMCFG4, 0xF6); //Modem Configuration   RX filter BW = 58Khz
  halRfWriteReg(MDMCFG3, 0x83); //Modem Configuration   26M*((256+83h)*2^6)/2^28 = 2.4kbps 
  halRfWriteReg(MDMCFG2, 0x02); //Modem Configuration   2-FSK;  no Manchester ; 16/16 sync word bits detected
  halRfWriteReg(MDMCFG1, 0x00); //Modem Configuration num preamble 2=>0 , Channel spacing_exp
  halRfWriteReg(MDMCFG0, 0x00); /*# MDMCFG0 Channel spacing = 25Khz*/
  halRfWriteReg(DEVIATN, 0x15);  //5.157471khz 
  halRfWriteReg(MCSM1, 0x00);
  halRfWriteReg(MCSM0, 0x18);   //Main Radio Control State Machine Configuration
  halRfWriteReg(FOCCFG, 0x1D);  //Frequency Offset Compensation Configuration
  halRfWriteReg(BSCFG, 0x1C);   //Bit Synchronization Configuration
  halRfWriteReg(AGCCTRL2, 0xC7);//AGC Control
  halRfWriteReg(AGCCTRL1, 0x00);//AGC Control
  halRfWriteReg(AGCCTRL0, 0xB2);//AGC Control
  halRfWriteReg(WORCTRL, 0xFB); //Wake On Radio Control
  halRfWriteReg(FREND1, 0xB6);  //Front End RX Configuration
  halRfWriteReg(FSCAL3, 0xE9);  //Frequency Synthesizer Calibration
  halRfWriteReg(FSCAL2, 0x2A);  //Frequency Synthesizer Calibration
  halRfWriteReg(FSCAL1, 0x00);  //Frequency Synthesizer Calibration
  halRfWriteReg(FSCAL0, 0x1F);  //Frequency Synthesizer Calibration
  halRfWriteReg(TEST2, 0x81);   //Various Test Settings link to adc retention
  halRfWriteReg(TEST1, 0x35);   //Various Test Settings link to adc retention
  halRfWriteReg(TEST0, 0x09);   //Various Test Settings link to adc retention

  SPIWriteBurstReg(PATABLE_ADDR, PA, 8);
}

bool cc1101_init(float freq)
{
  pinMode(GDO0, INPUT_PULLUP);

  if (wiringPiSPISetup(0, 500000) < 0) {
    Serial.println("CC1101: SPI init failed");
    return false;
  }
  cc1101_reset();
  delay(1); // 1ms for chip reset
  cc1101_configureRF_0(freq);
  return true;
}

int8_t cc1100_rssi_convert2dbm(uint8_t Rssi_dec)
{
  int8_t rssi_dbm;
  if (Rssi_dec >= 128)
  {
    rssi_dbm = ((Rssi_dec - 256) / 2) - 74;			//rssi_offset via datasheet
  }
  else
  {
    rssi_dbm = ((Rssi_dec) / 2) - 74;
  }
  return rssi_dbm;
}

/* configure cc1101 in receive mode */
void cc1101_rec_mode(void)
{
  uint8_t marcstate;
  CC1101_CMD(SIDLE);								//sets to idle first. must be in
  CC1101_CMD(SRX);									//writes receive strobe (receive mode)
  marcstate = 0xFF;									//set unknown/dummy state value
  while ((marcstate != 0x0D) && (marcstate != 0x0E) && (marcstate != 0x0F))							//0x0D = RX 
  {
    marcstate = halRfReadReg(MARCSTATE_ADDR);			//read out state of cc1100 to be sure in RX
  }
}

void echo_cc1101_version(void)
{
  echo_debug(debug_out, "CC1101 Partnumber: 0x%02X\n", halRfReadReg(PARTNUM_ADDR));
  echo_debug(debug_out, "CC1101 Version != 00 or 0xFF  : 0x%02X\n", halRfReadReg(VERSION_ADDR));  // != 00 or 0xFF
}

#define CFG_REGISTER  			0x2F									//47 registers
void show_cc1101_registers_settings(void)
{
  uint8_t config_reg_verify[CFG_REGISTER], Patable_verify[8];
  uint8_t i;

  memset(config_reg_verify, 0, CFG_REGISTER);
  memset(Patable_verify, 0, 8);

  SPIReadBurstReg(0, config_reg_verify, CFG_REGISTER);			//reads all 47 config register from cc1100	"359.63us"
  SPIReadBurstReg(PATABLE_ADDR, Patable_verify, 8);				//reads output power settings from cc1100	"104us"

  echo_debug(debug_out, "Config Register in hex:\n");
  echo_debug(debug_out, " 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
  for (i = 0; i < CFG_REGISTER; i++) 		//showes rx_buffer for debug
  {
    echo_debug(debug_out, "%02X ", config_reg_verify[i]);

    if (i == 15 || i == 31 || i == 47 || i == 63)			//just for beautiful output style
    {
      echo_debug(debug_out, "\n");
    }
  }
  echo_debug(debug_out, "\n");
  echo_debug(debug_out, "PaTable:\n");

  for (i = 0; i < 8; i++) 					//showes rx_buffer for debug
  {
    echo_debug(debug_out, "%02X ", Patable_verify[i]);
  }
  echo_debug(debug_out, "\n");

}

uint8_t is_look_like_radian_frame(uint8_t* buffer, size_t len)
{
  uint8_t ret = FALSE;
  for (size_t i = 0; i < len; i++) {
    if (buffer[i] == 0xFF) ret = TRUE;
  }

  return ret;
}

//-----------------[check if Packet is received]-------------------------
uint8_t cc1101_check_packet_received(void)
{
  uint8_t rxBuffer[100];
  uint8_t l_nb_byte, l_Rssi_dbm, l_lqi, l_freq_est, pktLen;
  pktLen = 0;
  if (digitalRead(GDO0) == TRUE)
  {
    // get RF info at beginning of the frame
    l_lqi = halRfReadReg(LQI_ADDR);
    l_freq_est = halRfReadReg(FREQEST_ADDR);
    l_Rssi_dbm = cc1100_rssi_convert2dbm(halRfReadReg(RSSI_ADDR));

    while (digitalRead(GDO0) == TRUE)
    {
      delay(5); //wait for some byte received
      l_nb_byte = (halRfReadReg(RXBYTES_ADDR) & RXBYTES_MASK);
      if ((l_nb_byte) && ((pktLen + l_nb_byte) < 100))
      {
        SPIReadBurstReg(RX_FIFO_ADDR, &rxBuffer[pktLen], l_nb_byte); // Pull data
        pktLen += l_nb_byte;
      }
    }
    if (is_look_like_radian_frame(rxBuffer, pktLen))
    {
      echo_debug(debug_out, "\n");
      print_time();
      echo_debug(debug_out, " bytes=%u rssi=%u lqi=%u F_est=%u ", pktLen, l_Rssi_dbm, l_lqi, l_freq_est);
      show_in_hex_one_line(rxBuffer, pktLen);
    }
    else
    {
      echo_debug(debug_out, ".");
    }
    fflush(stdout);
    return TRUE;
  }
  return FALSE;
}

uint8_t cc1101_wait_for_packet(int milliseconds)
{
  int i;
  for (i = 0; i < milliseconds; i++)
  {
    delay(1); //in ms	
    if (i % 100 == 0) ESP.wdtFeed();
    if (cc1101_check_packet_received()) //delay till system has data available
    {
      return TRUE;
    }
    else if (i == milliseconds - 1)
      return FALSE;
  }
  return TRUE;
}

struct tmeter_data parse_meter_report(uint8_t *decoded_buffer, uint8_t size)
{
  struct tmeter_data data;
  memset(&data, 0, sizeof(data));
  if (size >= METER_REPORT_MIN_SIZE_LITERS)
  {
    data.liters = decoded_buffer[METER_REPORT_OFFSET_LITERS_0]
        + (decoded_buffer[METER_REPORT_OFFSET_LITERS_1] << 8)
        + (decoded_buffer[METER_REPORT_OFFSET_LITERS_2] << 16)
        + (decoded_buffer[METER_REPORT_OFFSET_LITERS_3] << 24);
  }
  if (size >= METER_REPORT_MIN_SIZE_EXTRA)
  {
    data.reads_counter = decoded_buffer[METER_REPORT_OFFSET_READS_CTR];
    data.battery_left = decoded_buffer[METER_REPORT_OFFSET_BATTERY];
    data.time_start = decoded_buffer[METER_REPORT_OFFSET_TIME_START];
    data.time_end = decoded_buffer[METER_REPORT_OFFSET_TIME_END];
  }
  return data;
}

// Remove the start- and stop-bits in the bitstream , also decode oversampled bit 0xF0 => 1,0
// 01234567 ###01234 567###01 234567## #0123456 (# -> Start/Stop bit)
// is decoded to:
// 76543210 76543210 76543210 76543210
uint8_t decode_4bitpbit_serial(uint8_t *rxBuffer, int l_total_byte, uint8_t* decoded_buffer, int decoded_buffer_max_len)
{
  uint16_t i, j, k;
  uint8_t bit_cnt = 0;
  int8_t bit_cnt_flush_S8 = 0;
  uint8_t bit_pol = 0;
  uint8_t dest_bit_cnt = 0;
  uint8_t dest_byte_cnt = 0;
  uint8_t current_Rx_Byte;
  if (decoded_buffer_max_len <= 0) return 0;
  //show_in_hex(rxBuffer,l_total_byte);
  /*set 1st bit polarity*/
  bit_pol = (rxBuffer[0] & 0x80); //initialize with 1st bit state

  for (i = 0; i < l_total_byte; i++)
  {
    current_Rx_Byte = rxBuffer[i];
    //echo_debug(debug_out, "0x%02X ", rxBuffer[i]);
    for (j = 0; j < 8; j++)
    {
      if ((current_Rx_Byte & 0x80) == bit_pol) bit_cnt++;
      else if (bit_cnt == 1)
      { //previous bit was a glich so bit has not really change
        bit_pol = current_Rx_Byte & 0x80; //restore correct bit polarity
        bit_cnt = bit_cnt_flush_S8 + 1; //hope that previous bit was correctly decoded
      }
      else
      {  //bit polarity has change 
        bit_cnt_flush_S8 = bit_cnt;
        bit_cnt = (bit_cnt + 2) / 4;
        bit_cnt_flush_S8 = bit_cnt_flush_S8 - (bit_cnt * 4);

        for (k = 0; k < bit_cnt; k++)
        { // insert the number of decoded bit
          if (dest_bit_cnt < 8 && dest_byte_cnt < (uint8_t)decoded_buffer_max_len)
          { //if data byte and within buffer
            decoded_buffer[dest_byte_cnt] = decoded_buffer[dest_byte_cnt] >> 1;
            decoded_buffer[dest_byte_cnt] |= bit_pol;
          }
          dest_bit_cnt++;
          if ((dest_bit_cnt == 10) && (!bit_pol)) { echo_debug(debug_out, "stop bit error10"); return dest_byte_cnt; }
          if ((dest_bit_cnt >= 11) && (!bit_pol)) //start bit
          {
            dest_bit_cnt = 0;
            if (dest_byte_cnt >= (uint8_t)(decoded_buffer_max_len - 1)) return dest_byte_cnt; // overflow protection
            dest_byte_cnt++;
          }
        }
        bit_pol = current_Rx_Byte & 0x80;
        bit_cnt = 1;
      }
      current_Rx_Byte = current_Rx_Byte << 1;
    }//scan TX_bit
  }//scan TX_byte
  return dest_byte_cnt;
}

/*
   search for 0101010101010000b sync pattern then change data rate in order to get 4bit per bit
   search for end of sync pattern with start bit 1111111111110000b
   */
int receive_radian_frame(int size_byte, int rx_tmo_ms, uint8_t*rxBuffer, int rxBuffer_size)
{
  uint8_t  l_byte_in_rx = 0;
  uint16_t l_total_byte = 0;
  uint16_t l_radian_frame_size_byte = ((size_byte * (8 + 3)) / 8) + 1;
  int l_tmo = 0;
  uint8_t l_Rssi_dbm, l_lqi, l_freq_est;

  echo_debug(debug_out, "\nsize_byte=%d  l_radian_frame_size_byte=%d\n", size_byte, l_radian_frame_size_byte);

  if (l_radian_frame_size_byte * 4 > rxBuffer_size) { echo_debug(debug_out, "buffer too small\n"); return 0; }
  CC1101_CMD(SFRX);
  halRfWriteReg(MCSM1, 0x0F);   //CCA always ; default mode RX
  halRfWriteReg(MDMCFG2, 0x02); //Modem Configuration   2-FSK;  no Manchester ; 16/16 sync word bits detected
  halRfWriteReg(SYNC1, RADIAN_SYNC_PHASE1_H);
  halRfWriteReg(SYNC0, RADIAN_SYNC_PHASE1_L);
  halRfWriteReg(MDMCFG4, 0xF6); //Modem Configuration   RX filter BW = 58Khz
  halRfWriteReg(MDMCFG3, 0x83); //Modem Configuration   26M*((256+83h)*2^6)/2^28 = 2.4kbps	
  halRfWriteReg(PKTLEN, 1); // just one byte of synch pattern
  cc1101_rec_mode();

  Serial.printf("Waiting for GDO0 signal (phase 1, timeout: %dms)...\n", rx_tmo_ms);
  
  while ((digitalRead(GDO0) != GDO0_SIGNAL_LEVEL) && (l_tmo < rx_tmo_ms)) {
    delay(1); l_tmo++;
    if (l_tmo % 50 == 0) ESP.wdtFeed();
  }
  
  if (l_tmo < rx_tmo_ms) {
    echo_debug(debug_out, "GDO0! (0, %d) ", l_tmo);
    Serial.printf("GDO0 signal detected after %dms\n", l_tmo);
    
    if (l_tmo <= 2) {
      Serial.println("WARNING: Very fast GDO0 trigger detected - validating signal quality...");
      bool signal_stable = true;
      for (int i = 0; i < 5; i++) {
        delay(2);
        if (digitalRead(GDO0) != GDO0_SIGNAL_LEVEL) {
          signal_stable = false;
          break;
        }
      }
      
      if (!signal_stable) {
        Serial.println("❌ Signal validation failed - appears to be noise/glitch");
        return 0; // Abort this attempt
      } else {
        Serial.println("✅ Signal validated - proceeding despite fast trigger");
      }
    }
  } else {
    Serial.printf("GDO0 timeout after %dms - no RF signal detected\n", l_tmo);
    return 0;
  }
  while ((l_byte_in_rx == 0) && (l_tmo < rx_tmo_ms))
  {
    delay(5); l_tmo += 5; //wait for some byte received
    ESP.wdtFeed(); // Feed watchdog during receive wait
    l_byte_in_rx = (halRfReadReg(RXBYTES_ADDR) & RXBYTES_MASK);
    if (l_byte_in_rx)
    {
      SPIReadBurstReg(RX_FIFO_ADDR, &rxBuffer[0], l_byte_in_rx); // Pull data
      //if (debug_out)show_in_hex_one_line(rxBuffer, l_byte_in_rx);
    }
  }
  if (l_tmo < rx_tmo_ms && l_byte_in_rx > 0) echo_debug(debug_out, "1st synch received (%d) ", l_byte_in_rx); else return 0;

  l_lqi = halRfReadReg(LQI_ADDR);
  l_freq_est = halRfReadReg(FREQEST_ADDR);
  l_Rssi_dbm = cc1100_rssi_convert2dbm(halRfReadReg(RSSI_ADDR));
  echo_debug(debug_out, " rssi=%u lqi=%u F_est=%u \n", l_Rssi_dbm, l_lqi, l_freq_est);

  fflush(stdout);
  halRfWriteReg(SYNC1, RADIAN_SYNC_PHASE2_H);
  halRfWriteReg(SYNC0, RADIAN_SYNC_PHASE2_L);
  halRfWriteReg(MDMCFG4, 0xF8); //Modem Configuration   RX filter BW = 58Khz
  halRfWriteReg(MDMCFG3, 0x83); //Modem Configuration   26M*((256+83h)*2^8)/2^28 = 9.59kbps
  halRfWriteReg(PKTCTRL0, 0x02); //infinite packet len
  CC1101_CMD(SFRX);
  cc1101_rec_mode();

  l_total_byte = 0;
  l_byte_in_rx = 1;
  Serial.printf("Waiting for GDO0 signal (phase 2, remaining timeout: %dms)...\n", rx_tmo_ms - l_tmo);
  while ((digitalRead(GDO0) != GDO0_SIGNAL_LEVEL) && (l_tmo < rx_tmo_ms)) {
    delay(1); l_tmo++;
    if (l_tmo % 50 == 0) ESP.wdtFeed();
  }
  if (l_tmo < rx_tmo_ms) {
    echo_debug(debug_out, "GDO0! (1, %d) ", l_tmo);
    Serial.printf("GDO0 signal detected for data frame after %dms total\n", l_tmo);
    
    // Check for suspiciously fast triggers in phase 2 as well  
    // (Note: l_tmo is cumulative, so very fast phase 2 would show as total time close to phase 1 time)
    Serial.println("Phase 2 GDO0 signal received - checking data...");
  } else {
    Serial.printf("GDO0 timeout for data frame after %dms total\n", l_tmo);
    return 0;
  }
  while ((l_total_byte < (l_radian_frame_size_byte * 4)) && (l_tmo < rx_tmo_ms))
  {
    delay(5); l_tmo += 5; //wait for some byte received
    ESP.wdtFeed(); // Feed watchdog during frame receive
    l_byte_in_rx = (halRfReadReg(RXBYTES_ADDR) & RXBYTES_MASK);
    if (l_byte_in_rx)
    {
      //if (l_byte_in_rx + l_total_byte > (l_radian_frame_size_byte * 4))
      //  l_byte_in_rx = (l_radian_frame_size_byte * 4) - l_total_byte;

      SPIReadBurstReg(RX_FIFO_ADDR, &rxBuffer[l_total_byte], l_byte_in_rx); // Pull data
      l_total_byte += l_byte_in_rx;
    }
  }
  if (l_tmo < rx_tmo_ms && l_total_byte > 0) echo_debug(debug_out, "frame received (%d)\n", l_total_byte); else return 0;

  /*stop reception*/
  CC1101_CMD(SFRX);
  CC1101_CMD(SIDLE);
  //echo_debug(debug_out,"RAW buffer");
  //show_in_hex_array(rxBuffer,l_total_byte); //16ms pour 124b->682b , 7ms pour 18b->99byte
  /*restore default reg */
  halRfWriteReg(MDMCFG4, 0xF6); //Modem Configuration   RX filter BW = 58Khz
  halRfWriteReg(MDMCFG3, 0x83); //Modem Configuration   26M*((256+83h)*2^6)/2^28 = 2.4kbps
  halRfWriteReg(PKTCTRL0, 0x00); //fix packet len
  halRfWriteReg(PKTLEN, 38);
  halRfWriteReg(SYNC1, RADIAN_SYNC_DEFAULT_H);
  halRfWriteReg(SYNC0, RADIAN_SYNC_DEFAULT_L);
  return l_total_byte;
}

/*
   scenario_releve
   2s de WUP
130ms : trame interrogation de l'outils de reléve   ______------|...............-----
43ms de bruit
34ms 0101...01
14.25ms 000...000
14ms 1111...11111
83.5ms de data acquitement
50ms de 111111
34ms 0101...01
14.25ms 000...000
14ms 1111...11111
582ms de data avec l'index

l'outils de reléve doit normalement acquité
*/
struct tmeter_data get_meter_data(void)
{
  struct tmeter_data sdata;
  uint8_t marcstate = 0xFF;
  uint8_t wupbuffer[] = { 0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55 };
  uint8_t wup2send = 77;
  uint16_t tmo = 0;
  static uint8_t rxBuffer[1000];  // Make static to avoid stack overflow
  int rxBuffer_size;
  static uint8_t meter_data[200]; // Make static to avoid stack overflow
  uint8_t meter_data_size = 0;

  memset(&sdata, 0, sizeof(sdata));
  memset(rxBuffer, 0, sizeof(rxBuffer));    // Clear static buffer
  memset(meter_data, 0, sizeof(meter_data)); // Clear static buffer

  Serial.println("=== Starting meter data collection ===");
  Serial.printf("Initial CC1101 state check...\n");
  
  // Check GDO0 pin state before operations
  Serial.printf("GDO0 pin initial state: %s\n", digitalRead(GDO0) ? "HIGH" : "LOW");
  
  // If GDO0 is already HIGH, there might be a hardware issue or noise
  if (digitalRead(GDO0) == HIGH) {
    Serial.println("WARNING: GDO0 is HIGH before RF operations - possible noise or hardware issue");
    delay(10); // Brief delay to see if it settles
    Serial.printf("GDO0 after 10ms delay: %s\n", digitalRead(GDO0) ? "HIGH" : "LOW");
  }

  uint8_t txbuffer[100];
  Make_Radian_Master_req(txbuffer, METER_YEAR, METER_SERIAL);
  Serial.printf("Wake-up command prepared, starting transmission...\n");

  halRfWriteReg(MDMCFG2, 0x00);  //clear MDMCFG2 to do not send preamble and sync
  halRfWriteReg(PKTCTRL0, 0x02); //infinite packet len
  SPIWriteBurstReg(TX_FIFO_ADDR, wupbuffer, 8); wup2send--;
  CC1101_CMD(STX);	 //sends the data store into transmit buffer over the air
  delay(10); //to give time for calibration 
  marcstate = halRfReadReg(MARCSTATE_ADDR); //to  update 	CC1101_status_state
  echo_debug(debug_out, "MARCSTATE : raw:0x%02X  0x%02X free_byte:0x%02X sts:0x%02X sending 2s WUP...\n", marcstate, marcstate & 0x1F, CC1101_status_FIFO_FreeByte, CC1101_status_state);
  while ((CC1101_status_state == 0x02) && (tmo < TX_LOOP_OUT))					//in TX
  {
    // Feed watchdog to prevent reset during long operations
    ESP.wdtFeed();
    
    if (wup2send)
    {
      if (wup2send < 0xFF)
      {
        if (CC1101_status_FIFO_FreeByte <= 10)
        { //this give 10+20ms from previous frame : 8*8/2.4k=26.6ms  temps pour envoyer un wupbuffer
          delay(20);
          ESP.wdtFeed(); // Feed watchdog after delay
          tmo++; tmo++;
        }
        SPIWriteBurstReg(TX_FIFO_ADDR, wupbuffer, 8);
        wup2send--;
      }
    }
    else
    {
      delay(130); //130ms time to free 39bytes FIFO space
      ESP.wdtFeed(); // Feed watchdog after long delay
      SPIWriteBurstReg(TX_FIFO_ADDR, txbuffer, 39);
      wup2send = 0xFF;
    }
    delay(10); tmo++;
    ESP.wdtFeed(); // Feed watchdog in main loop
    marcstate = halRfReadReg(MARCSTATE_ADDR); //read out state of cc1100 to be sure in IDLE and TX is finished this update also CC1101_status_state
    //echo_debug(debug_out,"%ifree_byte:0x%02X sts:0x%02X\n",tmo,CC1101_status_FIFO_FreeByte,CC1101_status_state);			
  }
  echo_debug(debug_out, "%i free_byte:0x%02X sts:0x%02X\n", tmo, CC1101_status_FIFO_FreeByte, CC1101_status_state);
  Serial.printf("Transmission completed after %d loops. Final state: 0x%02X\n", tmo, CC1101_status_state);
  
  CC1101_CMD(SFTX); //flush the Tx_fifo content this clear the status state and put sate machin in IDLE
  //end of transition restore default register
  halRfWriteReg(MDMCFG2, 0x02); //Modem Configuration   2-FSK;  no Manchester ; 16/16 sync word bits detected   
  halRfWriteReg(PKTCTRL0, 0x00); //fix packet len

  //delay(30); //43ms de bruit
  /*34ms 0101...01  14.25ms 000...000  14ms 1111...11111  83.5ms de data acquitement*/
  Serial.println("Listening for first response (ACK frame, 18 bytes, 150ms timeout)...");
  uint8_t rssi1 = halRfReadReg(RSSI_ADDR);
  uint8_t lqi1 = halRfReadReg(LQI_ADDR);
  Serial.printf("Pre-receive RSSI: %d dBm, LQI: %d\n", cc1100_rssi_convert2dbm(rssi1), lqi1);
  
  if (!receive_radian_frame(RADIAN_ACK_FRAME_SIZE, 150, rxBuffer, sizeof(rxBuffer))) {
    echo_debug(debug_out, "TMO on REC\n");
    Serial.println("First frame timeout - no ACK received from meter");
  } else {
    Serial.println("First frame (ACK) received successfully");
  }
  
  //delay(30); //50ms de 111111  , mais on a 7+3ms de printf et xxms calculs
  /*34ms 0101...01  14.25ms 000...000  14ms 1111...11111  582ms de data avec l'index */
  Serial.println("Listening for second response (DATA frame, 124 bytes, 1000ms timeout)...");
  uint8_t rssi2 = halRfReadReg(RSSI_ADDR);
  uint8_t lqi2 = halRfReadReg(LQI_ADDR);
  Serial.printf("Pre-receive RSSI: %d dBm, LQI: %d\n", cc1100_rssi_convert2dbm(rssi2), lqi2);
  
  rxBuffer_size = receive_radian_frame(RADIAN_DATA_FRAME_SIZE, 1000, rxBuffer, sizeof(rxBuffer));
  if (rxBuffer_size)
  {
    Serial.printf("Second frame (DATA) received successfully - %d bytes\n", rxBuffer_size);
    meter_data_size = decode_4bitpbit_serial(rxBuffer, rxBuffer_size, meter_data, (int)sizeof(meter_data));
    Serial.printf("Decoded data size: %d bytes\n", meter_data_size);
    sdata = parse_meter_report(meter_data, meter_data_size);
    Serial.printf("Parsed meter data - Liters: %d, Counter: %d, Battery: %d months\n", 
                  sdata.liters, sdata.reads_counter, sdata.battery_left);
  }
  else
  {
    echo_debug(debug_out, "TMO on REC\n");
    Serial.println("Second frame timeout - no DATA received from meter");
  }
  
  // Read final RF status
  sdata.rssi = halRfReadReg(RSSI_ADDR); // Read RSSI value from CC1101
  sdata.rssi_dbm = cc1100_rssi_convert2dbm(halRfReadReg(RSSI_ADDR));  // Read RSSI value from CC1101 and convert to dBm
  sdata.lqi = halRfReadReg(LQI_ADDR); // Read LQI value from CC1101
  
  Serial.printf("Final RF status - RSSI: %d dBm, LQI: %d\n", sdata.rssi_dbm, sdata.lqi);
  Serial.println("=== Meter data collection completed ===");
  
  return sdata;
}

/*
 * Advanced meter data collection with frequency scanning
 * This function tries multiple frequencies to find the one that works
 * Addresses frequency drift issues common in aging water meters
 */
struct tmeter_data get_meter_data_with_frequency_scan(void)
{
  struct tmeter_data sdata;
  float base_frequency = FREQUENCY; // Base frequency from private.h
  float frequencies_to_try[81]; // Expanded array for comprehensive coverage
  int freq_count = 0;
  
  Serial.println("=== Starting COMPREHENSIVE meter data collection with thorough frequency scan ===");
  Serial.println("🔍 PRIORITY: Connection quality over speed - comprehensive scanning enabled");
  
  // Initialize return structure
  memset(&sdata, 0, sizeof(sdata));
  sdata.successful_frequency = 0.0f; // Mark as no success initially
  
  // Build comprehensive frequency list for thorough scanning
  // Try configured frequency first (most likely to work)
  frequencies_to_try[freq_count++] = base_frequency;
  
  // Phase 1: Fine-grained scan around base frequency (±10 kHz in 1 kHz steps)
  Serial.println("📡 Building frequency list - Phase 1: Fine scan around base frequency");
  for (float offset = 0.001f; offset <= 0.010f && freq_count < 40; offset += 0.001f) {
    frequencies_to_try[freq_count++] = base_frequency + offset; // Higher frequencies
    frequencies_to_try[freq_count++] = base_frequency - offset; // Lower frequencies
  }
  
  // Phase 2: Medium range scan (±50 kHz in 2.5 kHz steps, skipping already covered area)
  Serial.println("📡 Building frequency list - Phase 2: Medium range scan");
  for (float offset = 0.012f; offset <= 0.050f && freq_count < 70; offset += 0.0025f) {
    frequencies_to_try[freq_count++] = base_frequency + offset; // Higher frequencies  
    frequencies_to_try[freq_count++] = base_frequency - offset; // Lower frequencies
  }
  
  // Phase 3: Wide range scan (±100 kHz in 5 kHz steps, for major frequency drift)
  Serial.println("📡 Building frequency list - Phase 3: Wide range scan for major drift");
  for (float offset = 0.055f; offset <= 0.100f && freq_count < 80; offset += 0.005f) {
    frequencies_to_try[freq_count++] = base_frequency + offset; // Higher frequencies
    frequencies_to_try[freq_count++] = base_frequency - offset; // Lower frequencies
  }
  
  Serial.printf("🎯 Comprehensive scan range: %.6f to %.6f MHz (%d frequencies)\n", 
                base_frequency - 0.100f, base_frequency + 0.100f, freq_count);
  Serial.printf("⏱️  Estimated scan time: %d-%d minutes (depending on interference)\n", 
                (freq_count * 20) / 60, (freq_count * 45) / 60);
  
  // Try each frequency with comprehensive testing
  struct tmeter_data best_candidate;
  memset(&best_candidate, 0, sizeof(best_candidate));
  float best_candidate_freq = 0.0f;
  int8_t best_candidate_rssi = -127; // Minimum valid int8_t value for RSSI
  
  for (int i = 0; i < freq_count; i++) {
    float test_freq = frequencies_to_try[i];
    
    Serial.printf("\n--- Testing frequency %.6f MHz (attempt %d/%d) ---\n", test_freq, i+1, freq_count);
    
    // Initialize CC1101 with test frequency
    if (!cc1101_init(test_freq)) {
      Serial.printf("CC1101 init failed at %.6f MHz, skipping\n", test_freq);
      continue;
    }
    delay(100); // Extended delay for better radio settling
    ESP.wdtFeed();
    
    // Put CC1101 in receive mode to get valid RSSI reading
    cc1101_rec_mode();
    delay(20); // Extended delay for RSSI stabilization
    ESP.wdtFeed();
    
    // Take multiple RSSI readings for accuracy
    int16_t rssi_sum = 0; // Fix: Use int16_t to prevent overflow when summing negative values
    int valid_readings = 0;
    Serial.printf("📊 Noise floor analysis: ");
    
    for (int j = 0; j < 5; j++) {
      uint8_t rssi_raw = halRfReadReg(RSSI_ADDR);
      int8_t rssi_dbm = cc1100_rssi_convert2dbm(rssi_raw);
      if (rssi_dbm < 10) { // Reject unrealistically high RSSI; int8_t range -128..127 is valid
        rssi_sum += rssi_dbm;
        valid_readings++;
        Serial.printf("%d ", rssi_dbm);
      }
      delay(5);
    }
    
    if (valid_readings == 0) {
      Serial.println("❌ No valid RSSI readings - hardware issue");
      continue;
    }
    
    int8_t average_rssi = (int8_t)(rssi_sum / valid_readings); // Fix: Cast back to int8_t after division
    Serial.printf("→ Average: %d dBm (%d readings)\n", average_rssi, valid_readings);
    
    // Enhanced quality assessment 
    if (average_rssi > -30) {
      Serial.printf("⚠️  Skipping - noise floor too high (%d dBm indicates strong interference)\n", average_rssi);
      continue;
    }
    
    if (average_rssi < -120) {
      Serial.printf("⚠️  Skipping - signal too weak (%d dBm, minimum viable is -120 dBm)\n", average_rssi);
      continue;
    }
    
    // Assess signal quality category (const char* to avoid heap fragmentation)
    const char* signal_quality;
    if (average_rssi > -70) signal_quality = "EXCELLENT";
    else if (average_rssi > -85) signal_quality = "GOOD";
    else if (average_rssi > -100) signal_quality = "FAIR";
    else if (average_rssi > -115) signal_quality = "POOR";
    else signal_quality = "MARGINAL";
    Serial.printf("📡 Signal quality: %s (%d dBm)\n", signal_quality, average_rssi);
    
    // Try communication - with retry for promising frequencies
    int communication_attempts = 1;
    if (average_rssi > -90) {
      communication_attempts = 3; // Retry good signals multiple times
      Serial.println("🔄 Good signal detected - attempting multiple communication tries");
    } else if (average_rssi > -105) {
      communication_attempts = 2; // Retry fair signals once
      Serial.println("🔄 Fair signal detected - attempting additional communication try");
    }
    
    struct tmeter_data best_attempt;
    memset(&best_attempt, 0, sizeof(best_attempt));
    bool got_data = false;
    
    for (int attempt = 1; attempt <= communication_attempts; attempt++) {
      if (communication_attempts > 1) {
        Serial.printf("   📞 Communication attempt %d/%d\n", attempt, communication_attempts);
      }
      
      sdata = get_meter_data();
      
      bool valid_data = (sdata.reads_counter > 0 && sdata.liters > 0);
      if (valid_data) {
        const char* connection_quality;
        if (sdata.rssi_dbm > -70 && sdata.lqi > 100) connection_quality = "EXCELLENT";
        else if (sdata.rssi_dbm > -85 && sdata.lqi > 80) connection_quality = "GOOD";
        else if (sdata.rssi_dbm > -100 && sdata.lqi > 50) connection_quality = "ADEQUATE";
        else connection_quality = "MARGINAL";
        Serial.printf("✅ SUCCESS! Frequency %.6f MHz - %s CONNECTION\n", test_freq, connection_quality);
        Serial.printf("📊 Data: %d liters, counter %d, RSSI %d dBm, LQI %d\n",
                      sdata.liters, sdata.reads_counter, sdata.rssi_dbm, sdata.lqi);
        if (sdata.rssi_dbm > -70 && sdata.lqi > 100) {
          sdata.successful_frequency = test_freq;
          Serial.printf("🎯 RECOMMENDATION: Update FREQUENCY in private.h to %.6f\n", test_freq);
          Serial.printf("🚀 High-quality connection found - ending scan early\n");
          return sdata;
        }
        if (sdata.rssi_dbm > -85 && sdata.lqi > 80) {
          sdata.successful_frequency = test_freq;
          Serial.printf("🎯 RECOMMENDATION: Update FREQUENCY in private.h to %.6f\n", test_freq);
          return sdata;
        }
        // Track best adequate connection but continue scanning for better
        if (!got_data || sdata.rssi_dbm > best_attempt.rssi_dbm) {
          best_attempt = sdata;
          best_attempt.successful_frequency = test_freq;
          got_data = true;
        }
        break; // Success on this frequency, move to next
      }
      
      // Small delay between communication attempts
      if (attempt < communication_attempts) {
        delay(200);
        ESP.wdtFeed();
      }
    }
    
    if (got_data) {
      Serial.printf("💾 Storing frequency %.6f MHz as candidate (RSSI: %d dBm)\n", 
                    test_freq, best_attempt.rssi_dbm);
      if (!best_candidate.reads_counter || best_attempt.rssi_dbm > best_candidate.rssi_dbm) {
        best_candidate = best_attempt;
        best_candidate_freq = test_freq;
        best_candidate_rssi = best_attempt.rssi_dbm;
      }
    } else {
      // Track best signal quality even without data (helps with troubleshooting)
      if (average_rssi > best_candidate_rssi && average_rssi > -105) {
        Serial.printf("📈 Best signal so far: %.6f MHz (%d dBm) - no data yet\n", test_freq, average_rssi);
        best_candidate_rssi = average_rssi;
        best_candidate_freq = test_freq;
      }
      Serial.printf("❌ No data at %.6f MHz (signal: %d dBm)\n", test_freq, average_rssi);
    }
    
    // Progress indicator  
    if ((i + 1) % 10 == 0) {
      Serial.printf("📊 Progress: %d/%d frequencies tested (%.1f%% complete)\n", 
                    i + 1, freq_count, ((float)(i + 1) / freq_count) * 100);
      ESP.wdtFeed();
    }
    
    // Longer delay between frequency attempts for stability
    delay(150);
    ESP.wdtFeed();
  }
  
  // Return best candidate if found
  if (best_candidate.reads_counter > 0) {
    Serial.printf("\n🎯 SCAN COMPLETE - Using best candidate: %.6f MHz\n", best_candidate_freq);
    Serial.printf("📊 Connection: %d liters, counter %d, RSSI %d dBm, LQI %d\n",
                  best_candidate.liters, best_candidate.reads_counter, 
                  best_candidate.rssi_dbm, best_candidate.lqi);
    Serial.printf("🎯 STRONG RECOMMENDATION: Update FREQUENCY in private.h to %.6f\n", best_candidate_freq);
    return best_candidate;
  }
  
  Serial.println("\n=== FREQUENCY SCAN COMPLETED - NO WORKING FREQUENCY FOUND ===");
  Serial.println("❌ No frequency in the tested range provided valid meter data");
  Serial.println("📋 TROUBLESHOOTING SUGGESTIONS:");
  Serial.println("   1. Check CC1101 wiring and antenna connection");
  Serial.println("   2. Ensure meter is in wake window (typically business hours on weekdays)");
  Serial.println("   3. Reduce distance between CC1101 and water meter");
  Serial.println("   4. Try a broader frequency range scan");
  Serial.printf("   5. Current base frequency %.6f MHz may be significantly off\n", base_frequency);
  
  // Return empty data
  memset(&sdata, 0, sizeof(sdata));
  return sdata;
}

