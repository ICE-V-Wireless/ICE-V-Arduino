/*
 * ICE.c - interface routines  ICE FPGA
 * 04-04-22 E. Brombaugh
 */

#include <string.h>
#include "ice.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "soc/spi_periph.h"
#include "esp_rom_gpio.h"
#include "hal/gpio_hal.h"

/**
  * @brief  SPI Interface pins
  */
#define ICE_SPI_HOST		SPI2_HOST
#define ICE_SPI_SCK_PIN		((gpio_num_t)5) //2
#define ICE_SPI_MISO_PIN	((gpio_num_t)4) //3
#define ICE_SPI_MOSI_PIN	((gpio_num_t)7) //0
#define ICE_SPI_CS_PIN		((gpio_num_t)6) //1
#define ICE_CDONE_PIN		((gpio_num_t)0) //5
#define ICE_CRST_PIN		((gpio_num_t)1) //4

#define ICE_SPI_CS_LOW()	gpio_set_level(ICE_SPI_CS_PIN,0)
#define ICE_SPI_CS_HIGH()	gpio_set_level(ICE_SPI_CS_PIN,1)
#define ICE_CRST_LOW()		gpio_set_level(ICE_CRST_PIN,0)
#define ICE_CRST_HIGH()		gpio_set_level(ICE_CRST_PIN,1)
#define ICE_CDONE_GET()		gpio_get_level(ICE_CDONE_PIN)
#define ICE_SPI_DUMMY_BYTE	0xFF
#define ICE_SPI_MAX_XFER	4096

static const char* TAG = "ice";
static spi_device_handle_t spi;

void ICE_Init(void)
{
    esp_err_t ret;
    spi_bus_config_t buscfg={
        .mosi_io_num=ICE_SPI_MOSI_PIN,
        .miso_io_num=ICE_SPI_MISO_PIN,
        .sclk_io_num=ICE_SPI_SCK_PIN,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz = ICE_SPI_MAX_XFER,
    };
    spi_device_interface_config_t devcfg={
        .mode=0,                                //SPI mode 0
        .clock_speed_hz=10*1000*1000,           //Clock out at 40 MHz
        .spics_io_num=-1,                       //CS pin not used
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
    };

    //Initialize the SPI bus
    ESP_LOGI(TAG, "Initialize SPI");
	gpio_reset_pin(ICE_SPI_MISO_PIN);
	gpio_reset_pin(ICE_SPI_MOSI_PIN);
	gpio_reset_pin(ICE_SPI_SCK_PIN);
    ret=spi_bus_initialize(ICE_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
	
    //Attach the SPI bus
    ret=spi_bus_add_device(ICE_SPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    //Initialize non-SPI GPIOs
	/* pins 4-7 must be reset prior to use to get out of JTAG mode */
    ESP_LOGI(TAG, "Initialize GPIO");
	gpio_reset_pin(ICE_SPI_CS_PIN);
    gpio_set_direction(ICE_SPI_CS_PIN, GPIO_MODE_OUTPUT);
	ICE_SPI_CS_HIGH();
	gpio_reset_pin(ICE_CRST_PIN);
	gpio_set_direction(ICE_CRST_PIN, GPIO_MODE_OUTPUT);
	ICE_CRST_HIGH();
	gpio_reset_pin(ICE_CDONE_PIN);
	gpio_set_direction(ICE_CDONE_PIN, GPIO_MODE_INPUT);
}

void ICE_SPI_WriteByte(uint8_t dat8)
{
    esp_err_t ret;
    spi_transaction_t t = {0};
	
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&dat8;              //The data is the cmd itself
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

uint8_t ICE_SPI_WriteReadByte(uint8_t tdat8)
{
	uint8_t rdat8;
    esp_err_t ret;
    spi_transaction_t t = {0};
	
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&tdat8;             //The data is the cmd itself
	t.rx_buffer=&rdat8;				//received data
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
	return rdat8;
}

/*
 * Read a byte from SPI with dummy write
 */
uint8_t ICE_SPI_ReadByte(void)
{
	uint8_t tdat8=0, rdat8;
    esp_err_t ret;
    spi_transaction_t t = {0};
	
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&tdat8;             //The data is the cmd itself
	t.rx_buffer=&rdat8;				//received data
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
	return rdat8;
}

/*
 * Write a block of bytes to the ICE SPI
 */
void ICE_SPI_WriteBlk(uint8_t *Data, uint32_t Count)
{
    esp_err_t ret;
    spi_transaction_t t = {0};
	uint32_t bytes;
	
	while(Count)
	{
		bytes = (Count > ICE_SPI_MAX_XFER) ? ICE_SPI_MAX_XFER : Count;
		
		memset(&t, 0, sizeof(spi_transaction_t));
		t.length=8*bytes;	
		t.tx_buffer=Data;               //The data is the cmd itself
		ret=spi_device_polling_transmit(spi, &t);  //Transmit!
		assert(ret==ESP_OK);            //Should have had no issues.
		
		Count -= bytes;
		Data += bytes;
	}
}

/*
 * Read a block of bytes from the ICE SPI
 */
void ICE_SPI_ReadBlk(uint8_t *Data, uint32_t Count)
{
    esp_err_t ret;
    spi_transaction_t t = {0};
	uint32_t bytes;
	
	while(Count)
	{
		bytes = (Count > ICE_SPI_MAX_XFER) ? ICE_SPI_MAX_XFER : Count;
		
		memset(&t, 0, sizeof(spi_transaction_t));
		t.length=8*bytes;
		t.rxlength = t.length;
		t.rx_buffer = Data;
		ret=spi_device_polling_transmit(spi, &t);  //Transmit!
		assert(ret==ESP_OK);            //Should have had no issues.
		
		Count -= bytes;
		Data += bytes;
	}
}

/*
 * Bitbang the SPI_SCK - used for new timing config
 */
void ICE_SPI_ClkToggle(uint32_t cycles)
{
  /* configure SCK pin for GPIO output */
    esp_rom_gpio_pad_select_gpio(ICE_SPI_SCK_PIN);
  gpio_set_direction(ICE_SPI_SCK_PIN, GPIO_MODE_OUTPUT);
  
  /* toggle for cycles */
  while(cycles--)
  {
    gpio_set_level(ICE_SPI_SCK_PIN, 1);
    gpio_set_level(ICE_SPI_SCK_PIN, 0);
  }
  
  /* restore SCK pin to SPI control */
    esp_rom_gpio_connect_out_signal(ICE_SPI_SCK_PIN, spi_periph_signal[ICE_SPI_HOST].spiclk_out, false, false);
    esp_rom_gpio_connect_in_signal(ICE_SPI_SCK_PIN, spi_periph_signal[ICE_SPI_HOST].spiclk_in, false);
}

/*
 * configure the FPGA
 */
#if 0
/* original version - not quite to the Lattice timing spec */
uint8_t ICE_FPGA_Config(uint8_t *bitmap, uint32_t size)
{
  uint32_t timeout;

  /* drop reset bit */
  ICE_CRST_LOW();
  
  /* delay */
  ets_delay_us(1);
  
  /* drop CS bit to signal slave mode */
  ICE_SPI_CS_LOW();
  
  /* delay */
  ets_delay_us(200);
  
  /* Wait for done bit to go inactive */
  timeout = 100;
  while(timeout && (ICE_CDONE_GET()==1))
  {
    timeout--;
  }
  if(!timeout)
  {
    /* Done bit didn't respond to Reset */
    return 1;
  }
  
  /* raise reset */
  ICE_CRST_HIGH();
  
  /* delay >1200us to allow FPGA to clear */
  ets_delay_us(2000);
    //vTaskDelay(1);
  
  /* send the bitstream */
  ICE_SPI_WriteBlk(bitmap, size);
#if 1
    /* new ending logic */
    /* raise CS */
  ICE_SPI_CS_HIGH();

    /* Quick send 160 dummy clocks */
  uint8_t dummy[20] = {0};
  ICE_SPI_WriteBlk(dummy, 20);

    /* error if DONE not asserted */
    if(ICE_CDONE_GET()==0)
      return 2;
#else
    /* old ending logic is too slow */
  /* send clocks while waiting for DONE to assert */
  timeout = 100;
  while(timeout && (ICE_CDONE_GET()==0))
  {
    ICE_SPI_WriteByte(ICE_SPI_DUMMY_BYTE);
    timeout--;
  }
  if(!timeout)
  {
    /* FPGA didn't configure correctly */
    ICE_SPI_CS_HIGH();
    return 2;
  }
  
  /* send at least 49 more clocks */
  ICE_SPI_WriteByte(ICE_SPI_DUMMY_BYTE);
  ICE_SPI_WriteByte(ICE_SPI_DUMMY_BYTE);
  ICE_SPI_WriteByte(ICE_SPI_DUMMY_BYTE);
  ICE_SPI_WriteByte(ICE_SPI_DUMMY_BYTE);
  ICE_SPI_WriteByte(ICE_SPI_DUMMY_BYTE);
  ICE_SPI_WriteByte(ICE_SPI_DUMMY_BYTE);
  ICE_SPI_WriteByte(ICE_SPI_DUMMY_BYTE);
  /* Raise CS bit for subsequent port transactions */
  ICE_SPI_CS_HIGH();
#endif

  /* no error handling for now */
  return 0;
}
#else
/* New version is closer to Lattice timing */
uint8_t ICE_FPGA_Config(uint8_t *bitmap, uint32_t size)
{
  uint32_t timeout;

  /* drop reset bit */
  ICE_CRST_LOW();
  
  /* delay */
  ets_delay_us(1);
  
  /* drop CS bit to signal slave mode */
  ICE_SPI_CS_LOW();
  
  /* delay at least 200ns */
  ets_delay_us(1);
  
  /* Wait for done bit to go inactive */
  timeout = 100;
  while(timeout && (ICE_CDONE_GET()==1))
  {
    timeout--;
  }
  if(!timeout)
  {
    /* Done bit didn't respond to Reset */
    return 1;
  }

  /* raise reset */
  ICE_CRST_HIGH();
  
  /* delay >1200us to allow FPGA to clear */
  ets_delay_us(1200);
  
  /* send 8 dummy clocks with CS high */
  ICE_SPI_CS_HIGH();
  ICE_SPI_ClkToggle(8);
  ICE_SPI_CS_LOW();
  
  /* send the bitstream */
  ICE_SPI_WriteBlk(bitmap, size);

    /* raise CS */
  ICE_SPI_CS_HIGH();

  /* bitbang clock */
  ICE_SPI_ClkToggle(160);

    /* error if DONE not asserted */
    if(ICE_CDONE_GET()==0)
    {
    return 2;
  }
  
  /* no error handling for now */
  return 0;
}
#endif

/*
 * Write a long to the FPGA SPI port
 */
void ICE_FPGA_Serial_Write(uint8_t Reg, uint32_t Data)
{
	/* Drop CS */
	ICE_SPI_CS_LOW();
	
	/* msbit of byte 0 is 0 for write */
	ICE_SPI_WriteByte(Reg & 0x7f);

	/* send next four bytes */
	ICE_SPI_WriteByte((Data>>24) & 0xff);
	ICE_SPI_WriteByte((Data>>16) & 0xff);
	ICE_SPI_WriteByte((Data>> 8) & 0xff);
	ICE_SPI_WriteByte((Data>> 0) & 0xff);
	
	/* Raise CS */
	ICE_SPI_CS_HIGH();
}

/*
 * Read a long from the FPGA SPI port
 */
void ICE_FPGA_Serial_Read(uint8_t Reg, uint32_t *Data)
{
	uint8_t rx[4];
	
	/* Drop CS */
	ICE_SPI_CS_LOW();
	
	/* msbit of byte 0 is 1 for write */
	ICE_SPI_WriteByte(Reg | 0x80);

	/* get next four bytes */
	rx[0] = ICE_SPI_ReadByte();
	rx[1] = ICE_SPI_ReadByte();
	rx[2] = ICE_SPI_ReadByte();
	rx[3] = ICE_SPI_ReadByte();
	
	/* assemble result */
	*Data = (rx[0]<<24) | (rx[1]<<16) | (rx[2]<<8) | rx[3];
	
	/* Raise CS */
	ICE_SPI_CS_HIGH();
}

/***********************************************************************/
/* I know that ESP32 SPI ports can do memory cmd/addr/data sequencing  */
/* but I'm handling it manually here to avoid constantly reconfiguring */
/***********************************************************************/
/*
 * Write a block of data to the FPGA attached PSRAM via SPI port
 */
void ICE_PSRAM_Write(uint32_t Addr, uint8_t *Data, uint32_t size)
{
	uint8_t header[4];
	
	/* build the PSRAM Write header */
	header[0] = 0x02;					// slow write command
	header[1] = (Addr >> 16) & 0xff;
	header[2] = (Addr >>  8) & 0xff;
	header[3] = (Addr >>  0) & 0xff;
	
	/* Drop CS */
	ICE_SPI_CS_LOW();
	
	/* send header */
	ICE_SPI_WriteBlk(header, 4);
	
	/* send data */
	ICE_SPI_WriteBlk(Data, size);
	
	/* Raise CS */
	ICE_SPI_CS_HIGH();	
}

/*
 * Read a block of data from the FPGA attached PSRAM via SPI port
 */
void ICE_PSRAM_Read(uint32_t Addr, uint8_t *Data, uint32_t size)
{
	uint8_t header[4];
	
	/* build the PSRAM Read header */
	header[0] = 0x03;					// slow read command
	header[1] = (Addr >> 16) & 0xff;
	header[2] = (Addr >>  8) & 0xff;
	header[3] = (Addr >>  0) & 0xff;
	
	/* Drop CS */
	ICE_SPI_CS_LOW();
	
	/* send header */
	ICE_SPI_WriteBlk(header, 4);
	
	/* get data */
	ICE_SPI_ReadBlk(Data, size);
	
	/* Raise CS */
	ICE_SPI_CS_HIGH();
}
