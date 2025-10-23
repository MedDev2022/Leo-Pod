#pragma once
#include "UartEndpoint.hpp"
#include <string>
#include <vector>


// Define the VISCA commands for zoom and focus
#define VISCA_ADDRESS 0x01  // Address of the camera
#define VISCA_ZOOM_IN 0x81
#define VISCA_ZOOM_OUT 0x91
#define VISCA_FOCUS_NEAR 0x82
#define VISCA_FOCUS_FAR 0x92

// Set the baud rate for serial communication
#define VISCA_BAUD_RATE 9600 // 115200 // 57600  // 38400 // 19200  // 4800, 2400,  9600

typedef enum {
	eIdle = 0,
	eZoomIRQsent,
	eZoomPosReceived,
	eZoomPosError,
	eZoomPosTimout,
	eFocusIRQsent,
	eFocusPosReceived,
	eFocusPosError,
	eFocusPosTimout
} EViscaRX_Status;

class DayCam : public UartEndpoint {
public:
    explicit DayCam(UART_HandleTypeDef* huart);
    void Init();
    void setAddress();
    void setProtocol();
    void ifClear();
    void handleZoomIn(uint8_t* speed, uint8_t length);
    void handleZoomOut(uint8_t* speed, uint8_t length);
    void handleZoom2Position(uint16_t position);
    void handleZoomStop();
    void handleFocusFar(uint8_t* speed, uint8_t length);
    void handleFocusNear(uint8_t* speed, uint8_t length);
    void handleFocus2Position(uint16_t position);
    void handleFocusStop();



private:


    uint8_t rxBuffer[64];  // Adjust size as necessary for your data

    uint8_t byte_;
protected:
    void onReceiveByte(uint8_t byte) override;
    void processIncoming() override;




public:


	uint8_t address_command[4] = 	{ 0x88, 0x30, 0x01, 0xFF };
	uint8_t if_clear[5] = 			{ 0x88, 0x01, 0x00, 0x01, 0xFF };

    uint8_t commandCancel[3] = { 0x81,0x21,0xFF };
    const uint16_t delayTime = 250;   //Time between commands

    // ZOOM
    uint8_t zoom_tele[6] = 		{ 0x81, 0x01, 0x04, 0x07, 0x02, 0xFF };   	//Not exactly sure how this works yet
    uint8_t zoom_wide[6] = 		{ 0x81, 0x01, 0x04, 0x07, 0x03, 0xFF };  	//Not exactly sure how this works yet
    uint8_t zoom_teleVar[6] = 	{ 0x81, 0x01, 0x04, 0x07, 0x25, 0xFF }; //Zoom In: 81 01 04 07 2x FF , where x is speed- 0(low)-7(hi);
    uint8_t zoom_wideVar[6] = 	{ 0x81, 0x01, 0x04, 0x07, 0x35, 0xFF }; //Zoom Out: 81 01 04 07 3x FF , where x is speed- 0(low)-7(hi);
    uint8_t zoom_stop[6] = 		{ 0x81, 0x01, 0x04, 0x07, 0x00, 0xFF };  	//Stop all zooming
    uint8_t zoom2Position[9] = { 0x81, 0x01, 0x04, 0x47, 0x00, 0x00, 0x00, 0x00, 0xFF }; // 8x 01 04 47 0p 0q 0r 0s FF, where pqrs Zoom Position 0x0000 - 0x4000


    uint8_t power_reset[6] = 	{ 0x81, 0x01, 0x04, 0x00, 0x03, 0xFF };
    uint8_t zoom_in[6] = 		{ 0x81, 0x01, 0x04, 0x07, 0x02, 0xFF };
    uint8_t zoom_out[6] = 		{ 0x81, 0x01, 0x04, 0x07, 0x03, 0xFF };


    // FOCUS
    uint8_t focus_far[6] = 		{ 0x81, 0x01, 0x04, 0x08, 0x02, 0xFF };
    uint8_t focus_near[6] = 	{ 0x81, 0x01, 0x04, 0x08, 0x03, 0xFF };
    uint8_t focus_farVar[6] = 	{ 0x81, 0x01, 0x04, 0x08, 0x25, 0xFF };    	// 8x 01 04 08 2p FF
    uint8_t focus_nearVar[6] = 	{ 0x81, 0x01, 0x04, 0x08, 0x35, 0xFF };   	// 8x 01 04 08 3p FF
    uint8_t focus_stop[6] = 	{ 0x81, 0x01, 0x04, 0x08, 0x00, 0xFF };
    uint8_t focus2Position[9] = { 0x81, 0x01, 0x04, 0x48, 0x00, 0x00, 0x00, 0x00, 0xFF }; // 8x 01 04 48 0p 0q 0r 0s FF, where pqrs Focus Position 0x0000 - 0x4000

    uint8_t focus_auto[6] = 	{ 0x81, 0x01, 0x04, 0x38, 0x02, 0xFF };
    uint8_t focus_manual[6] = 	{ 0x81, 0x01, 0x04, 0x38, 0x03, 0xFF };
    uint8_t focus_OnePushTrigger[6] = { 0x81, 0x01, 0x04, 0x18, 0x01, 0xFF };

    // QUERY - Zoom & Focus
    uint8_t zoom_PosInq[5] = 	{ 0x81, 0x09, 0x04, 0x47, 0xFF };
    uint8_t focus_PosInq[5] = 	{ 0x81, 0x09, 0x04, 0x48, 0xFF };

    // QUERY - Power
    uint8_t power_Inq[5] = 	{ 0x81, 0x09, 0x04, 0x00, 0xFF };

    //Response Power ON
    uint8_t res_Power_ON[4] = 	{ 0x90, 0x50, 0x20, 0xFF};

    //Day/Night switch command
    uint8_t cam_IRC_Night[6] = { 0x81, 0x01, 0x04, 0x01, 0x02, 0xFF };
    uint8_t cam_IRC_Day[6]   = { 0x81, 0x01, 0x04, 0x01, 0x03, 0xFF };


    //byte zoom_out[6] = { 0x81, 0x01, 0x04 ,0x47 ,0x0p ,0x0q ,0x0r ,0x0s ,0xFF };  //  pqrs : Zoom Position

    uint8_t test_visca[6] = { 0x81, 0x01, 0x09, 0x00, 0x02, 0xFF };  // test_visca

//    const int BTN_ZOOM_IN_PIN = 46;    // Zoom in  =  ZOOM_TELE
//    const int BTN_ZOOM_OUT_PIN = 47;   // Zoom out = ZOOM_WIDE
//    const int BTN_FOCUS_FAR_PIN = 48;   // Focus in  =  ZOOM_TELE
//    const int BTN_FOCUS_NEAR_PIN = 49;  // Focus out = ZOOM_WIDE


 //   c

};
