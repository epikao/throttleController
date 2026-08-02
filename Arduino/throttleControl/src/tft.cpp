#include <Arduino.h>
#include <SPI.h>
#include "globals.h" 
#include "tft.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"

#define TFT3 //TFT1 TN, TFT2 IPS 2inch, TFT3 IPS 2.4inch
//lv_disp_drv_t* lvgl_disp_drv = nullptr;



// Setzt nur den Prescaler CPSDVSR=2, die reale Taktrate wird in ui_setup() ueber SCR bestimmt.
// Werte unter ~33MHz wuerden CPSDVSR=4 ergeben und dort dann nur die halbe Frequenz liefern.
static SPISettings tft_spi_settings(40000000, MSBFIRST, SPI_MODE3);

static const uint16_t screenWidth = 240; 
static const uint16_t screenHeight = 320; 
static const uint32_t drawbuffer = screenWidth * screenHeight / 10;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[drawbuffer]; // Ein Puffer von 10 Zeilen. Anpassbar.
//static lv_color_t buf_2[drawbuffer]; // Ein Puffer von 10 Zeilen. Anpassbar.
//static lv_color_t __attribute__((aligned(4))) buf[screenWidth * 10];

void TFT_init(void);
void writecommand(uint8_t c);
void writedata(uint8_t c);
void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);


void ui_setup(){

    //LVGL init

    lv_init();

    //Buffer init
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, drawbuffer); /* Initialize the display buffer. 10 lines buffer */

    static lv_disp_drv_t disp_drv;          /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = disp_flush;      /*Set your driver function*/
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);        /*Finally register the driver*/

	//TFT BKL Init
    pinMode(BACKLIGHT_PIN,OUTPUT);
	digitalWrite(BACKLIGHT_PIN, HIGH);
	
    //TFT SPI init
    pinMode(TFT_DC_PIN, OUTPUT);
	pinMode(TFT_CS_PIN, OUTPUT);


	
#if 1	
	SPI.setSCK(TFT_SCK_PIN);    // Pin 18
    SPI.setMOSI(TFT_MOSI_PIN);  // Pin 19
	SPI.setMISO(TFT_MISO_PIN);  // Pin 16
	SPI.begin();
	//uint32_t sysClk = clock_get_hz(clk_sys);
	//clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, sysClk, sysClk);
    SPI.beginTransaction(tft_spi_settings);
	
	//SCR=0 -> f = clk_peri / (CPSDVSR * 1) = 133MHz / 2 = 66.5MHz (Maximum des RP2040)
	hw_write_masked(&spi_get_hw(spi0)->cr0, (1 - 1) << SPI_SSPCR0_SCR_LSB, SPI_SSPCR0_SCR_BITS);
	
	//Reset TFT
	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, HIGH);
	delay(1);
	digitalWrite(RESET_PIN, LOW);
	delay(50);
	digitalWrite(RESET_PIN, HIGH);
	delay(50);	

    TFT_init();
#endif
}

// LVGL-Callback to send ui data to the display
void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
#if 1
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint32_t size = w * h;
	
    //tft.startWrite();
    //tft.setAddrWindow(area->x1, area->y1, w, h);
    //tft.pushColors((uint16_t *)color_p, w * h, true);
    //tft.endWrite();

    uint32_t x1 = area->x1;
    uint32_t y1 = area->y1;
    uint32_t x2 = area->x1 + w -1;
    uint32_t y2 = area->y1 + h -1;
	
	//Serial.println("FLUSH DEBUG 1: Starte Adressierung...");
	
    writecommand(0x2A); //x pixel-position anfahren
    writedata(x1>>8); //start-position
    writedata(x1); //start-position
    writedata(x2>>8); //end-position
    writedata(x2); //end-position
    //writedata32(x1,x1>>8,x2>>8,x2);
	
	//Serial.println("FLUSH DEBUG 2: X-Adressierung fertig.");

    writecommand(0x2B); //y pixel-positon anfahren
    writedata((y1)>>8); //start-position
    writedata(y1); //start-position
    writedata((y2)>>8); //end-position
    writedata(y2); //end-position

	//Serial.println("FLUSH DEBUG 3: Y-Adressierung fertig.");

    //startDMATransfer(txData, sizeof(txData));
    writecommand(0x2C); //send RAM data

	digitalWrite(TFT_DC_PIN, HIGH);
	digitalWrite(TFT_CS_PIN, LOW);
	//sio_hw->gpio_clr = (1L<<TFT_CS_PIN);
    //sio_hw->gpio_set = (1L<<TFT_DC_PIN);
	//gpio_put(PIN_CS, 0); 
	spi_write_blocking(spi0, (uint8_t*)color_p, size * 2);

	//spi_write16_blocking(spi0, (uint16_t*)color_p, size );
	//SPI.transfer((uint8_t*)color_p, size * 2); 
    //sio_hw->gpio_set = (1L<<TFT_CS_PIN);
	digitalWrite(TFT_CS_PIN, HIGH);
	//gpio_put(PIN_CS, 1);
    lv_disp_flush_ready(disp);
#endif
}

void TFT_init(){

#if defined TFT1

	Serial.println("TFT Init");
	writecommand(0x11);	// Exit Sleep 
	delay(600); 

	writecommand(0x36);	// Memory Access Control 
	writedata(0x00);	

	writecommand(0x3a);	
	writedata(0x05);	//16bit    
	
	writecommand(0xb2);  
	writedata(0x0c); 
	writedata(0x0c); 
	writedata(0X00); 
	writedata(0X33); 
	writedata(0X33);

	
	writecommand(0xb7);  //VGH VGL
	writedata(0X35);    //
	
	writecommand(0xbb); //VCOM
	writedata(0X20);	//28

	writecommand(0xc0); 
	writedata(0X2c);	
		
	writecommand(0xc2); //
	writedata(0X01);
	writedata(0Xff);	
	
	writecommand(0xc3); //VRH  
	writedata(0X0b);
	
	writecommand(0xc4); //vdv
	writedata(0X20);
	
   	writecommand(0xc6); //frame rate
   	writedata(0X0f);

	writecommand(0xd0); 
	writedata(0Xa4);
	writedata(0X51);


	writecommand(0x2A); //Column address
	writedata(0X00);      
	writedata(0X00);   
	writedata(0X00);   //0x00ef=239
	writedata(0Xef);   
	writecommand(0x2b); //row address 
	writedata(0X00);      
	writedata(0X00);   
	writedata(0X01);   //0x013f=319
	writedata(0X3f);   
	
	writecommand(0xE0);	// Set Gamma 
	writedata(0xd0); 
	writedata(0x00);
	writedata(0x02);
	writedata(0x07); 
	writedata(0x0a); 
	writedata(0x28);
	writedata(0x32);
	writedata(0x44); 
	writedata(0x42); 
	writedata(0x06); 
	writedata(0x0e); 
	writedata(0x12); 
	writedata(0x14);
	writedata(0x17);
	 
	writecommand(0XE1);	// Set Gamma 
	writedata(0xd0); 
	writedata(0x00);
	writedata(0x02);
	writedata(0x07); 
	writedata(0x0a); 
	writedata(0x28); 
	writedata(0x31); 
	writedata(0x54); 
	writedata(0x47); 
	writedata(0x0e); 
	writedata(0x1c); 
	writedata(0x17); 
	writedata(0x1b); 
	writedata(0x1e); 
	
	writecommand(0x20);	

	delay(100); 
	writecommand(0x29);	// Display on 
	delay(100);
#endif

#if defined TFT2
delay(150);
writecommand(0x11); 
delay(120);      //Delay 120ms 

writecommand(0x36);
writedata(0x00);

writecommand(0x3a);
writedata(0x05);//
//---------------ST7789V Frame rate setting----------//
writecommand(0xb2);
writedata(0x0C);//05
writedata(0x0C);//05
writedata(0x00);
writedata(0x33);
writedata(0x33);

writecommand(0xb7);
writedata(0x35);
//----------ST7789V Power setting------------//
writecommand(0xbb);
writedata(0x20); //VCOM

writecommand(0xc0);
writedata(0x2c);

writecommand(0xc2);
writedata(0x01);

writecommand(0xc3);
writedata(0x08);

writecommand(0xc4);
writedata(0x20);

writecommand(0xc6);
writedata(0x0F);

writecommand(0xd0);
writedata(0xa4);
writedata(0xa1);

//--------------------GAMMA--------------------------------
writecommand(0xE0);     
writedata(0xD0);   
writedata(0x05);   
writedata(0x0A);   
writedata(0x09);   
writedata(0x08);   
writedata(0x05);   
writedata(0x2E);   
writedata(0x44);   
writedata(0x45);   
writedata(0x0F);   
writedata(0x17);   
writedata(0x16);   
writedata(0x2B);   
writedata(0x33);   

writecommand(0xE1);     
writedata(0xD0);   
writedata(0x05);   
writedata(0x0A);   
writedata(0x09);   
writedata(0x08);   
writedata(0x05);   
writedata(0x2E);   
writedata(0x43);   
writedata(0x45);   
writedata(0x0F);   
writedata(0x16);   
writedata(0x16);   
writedata(0x2B);   
writedata(0x33); 

writecommand(0x21); 

writecommand(0x2A); //Frame rate control
writedata(0x00);
writedata(0x00);
writedata(0x00);
writedata(0xEF);

writecommand(0x2B); //Display function control
writedata(0x00);
writedata(0x00);
writedata(0x01);
writedata(0x3F);

writecommand(0x29); //display on

writecommand(0x2c);

#endif

#if defined TFT3
writecommand(0x11);
delay(120);      //Delay 120ms

writecommand(0x3a);
writedata(0x05);

writecommand(0x36);
writedata(0x00);
//---------------ST7789S Frame rate setting----------//
writecommand(0xb2);
writedata(0x00);
writedata(0x00);
writedata(0x00);
writedata(0x33);
writedata(0x33);

writecommand(0xb7);
writedata(0x35);
//----------ST7789S Power setting------------//
writecommand(0xb8);
writedata(0x2f);
writedata(0x2b);
writedata(0x2f);

writecommand(0xbb);
writedata(0x24); //VCOM

writecommand(0xc0);
writedata(0x2c);

writecommand(0xc3);
writedata(0x10); //17 = heller/dunkler

writecommand(0xc4);
writedata(0x20);

writecommand(0xc6);
writedata(0x11);

writecommand(0xd0);
writedata(0xa4);
writedata(0xa1);

writecommand(0xe8);
writedata(0x03);

writecommand(0xe9);
writedata(0x0d);
writedata(0x12);
writedata(0x00);

//--------------------GAMMA--------------------------------
writecommand(0xE0);
writedata(0xD0);
writedata(0x00);
writedata(0x00);
writedata(0x08);
writedata(0x11);
writedata(0x1A);
writedata(0x2B);
writedata(0x33);
writedata(0x42);
writedata(0x26);
writedata(0x12);
writedata(0x21);
writedata(0x2F);
writedata(0x11);

writecommand(0xE1);
writedata(0xD0);
writedata(0x02);
writedata(0x09);
writedata(0x0D);
writedata(0x0D);
writedata(0x27);
writedata(0x2B);
writedata(0x33);
writedata(0x42);
writedata(0x17);
writedata(0x12);
writedata(0x11);
writedata(0x2F);
writedata(0x31);

writecommand(0x21); //Inversion on

writecommand(0x2A); //Frame rate control
writedata(0x00);
writedata(0x00);
writedata(0x00);
writedata(0xEF);

writecommand(0x2B); //Display function control
writedata(0x00);
writedata(0x00);
writedata(0x01);
writedata(0x3F);

writecommand(0x29); //display on

writecommand(0x2c);

#endif
}

void writecommand(uint8_t c){
    uint8_t cmd = c;
    digitalWrite(TFT_DC_PIN, LOW);
	digitalWrite(TFT_CS_PIN, LOW);
	//gpio_put(PIN_CS, 0); 
	//sio_hw->gpio_clr = (1L<<TFT_CS_PIN);
	//sio_hw->gpio_clr = (1L<<TFT_DC_PIN);
	//Serial.println("writecommand before spi");
	spi_write_blocking(spi0, &c, 1);
	//SPI.transfer(c); 
	//Serial.println("writecommand after spi");
	
	digitalWrite(TFT_CS_PIN, HIGH);
	//sio_hw->gpio_set = (1L<<TFT_CS_PIN);
	//gpio_put(PIN_CS, 1); 
}

void writedata(uint8_t d){
    uint8_t data = d;
    digitalWrite(TFT_DC_PIN, HIGH);
	digitalWrite(TFT_CS_PIN, LOW);
	//gpio_put(PIN_CS, 0); 
	//sio_hw->gpio_clr = (1L<<TFT_CS_PIN);
    //sio_hw->gpio_set = (1L<<TFT_DC_PIN);
	spi_write_blocking(spi0, &d, 1);
	//SPI.transfer(d); 
	
	digitalWrite(TFT_CS_PIN, HIGH);
	//sio_hw->gpio_set = (1L<<TFT_CS_PIN);
	//gpio_put(PIN_CS, 1); 
}