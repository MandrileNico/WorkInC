/*==================[inclusions]=============================================*/
// Standard C Included Files
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// SDK Included Files
#include "fsl_sim_hal.h"
#include "fsl_lpsci_hal.h"
#include "fsl_clock_manager.h"
#include "fsl_pit_hal.h"
#include "fsl_mcg_hal.h"
#include "fsl_port_hal.h"
#include "fsl_gpio_hal.h"
#include "fsl_smc_hal.h"
#include "fsl_device_registers.h"
#include "fsl_hwtimer_systick.h"

// Project Included Files
#include "board.h"
#include "key.h"
#include "mma8451.h"
#include "board.h"
#include "LCD.h"
#include "MKL46Z4.h"
#include "core_cmInstr.h"

/*==================[macros and definitions]=================================*/
typedef enum
{
	EST_MEF_CAIDA_INICIALIZACION = 0,
	EST_MEF_CAIDA_ESPERANDO_INTERRUPCION,
	EST_MEF_CAIDA_CAYENDO,
	EST_MEF_CAIDA_CAYO,

}estMefCaida_enum;

typedef enum
{
	EST_MEF_LED_ESPERANDO_ORDEN = 0,
	EST_MEF_LED_TOOGLE,

}estMefLed_enum;

/* OSC0 configuration. */
#define OSC0_XTAL_FREQ 8000000U

/* EXTAL0 PTA18 */
#define EXTAL0_PORT   PORTA
#define EXTAL0_PIN    18
#define EXTAL0_PINMUX kPortPinDisabled

/* XTAL0 PTA19 */
#define XTAL0_PORT   PORTA
#define XTAL0_PIN    19
#define XTAL0_PINMUX kPortPinDisabled

#define cero	0
/*==================[internal data declaration]==============================*/
void PIT_Init(void);
void mefCaida(void);
void mefLed(void);
int16_t MaxAcc(void);

/*=======================================================*/
/* Configuration for enter VLPR mode. Core clock = 4MHz. */
/*=======================================================*/
const clock_manager_user_config_t clockConfigVlpr =
{
    .mcgConfig =
    {
        .mcg_mode           = kMcgModeBLPI, // Work in BLPI mode.
        .irclkEnable        = true,  		// MCGIRCLK enable.
        .irclkEnableInStop  = false, 		// MCGIRCLK disable in STOP mode.
        .ircs               = kMcgIrcFast, 	// Select IRC4M.
		.fcrdiv             = 0,    		// FCRDIV 0. Divide Factor is 1

        .frdiv   = 0,						// No afecta en este modo
        .drs     = kMcgDcoRangeSelLow,  	// No afecta en este modo
        .dmx32   = kMcgDmx32Default,    	// No afecta en este modo

        .pll0EnableInFllMode = false,  		// No afecta en este modo
        .pll0EnableInStop  = false,  		// No afecta en este modo
        .prdiv0            = 0b00,			// No afecta en este modo
        .vdiv0             = 0b00,			// No afecta en este modo
    },
    .simConfig =
    {
        .pllFllSel = kClockPllFllSelPll,	// No afecta en este modo
        .er32kSrc  = kClockEr32kSrcLpo,     // ERCLK32K selection, use LPO.
        .outdiv1   = 0b0000,				// tener cuidado con frecuencias máximas de este modo
        .outdiv4   = 0b101,					// tener cuidado con frecuencias máximas de este modo
    },
    .oscerConfig =
    {
        .enable       = true,	  			// OSCERCLK enable.
        .enableInStop = true, 				// OSCERCLK enable in STOP mode.
    }
};

/*=======================================================*/
/* Configuration for enter RUN mode. Core clock = 48MHz. */
/*=======================================================*/
const clock_manager_user_config_t clockConfigRun =
{
    .mcgConfig =
    {
        .mcg_mode           = kMcgModePEE,  // Work in PEE mode.
        .irclkEnable        = true,  		// MCGIRCLK enable.
        .irclkEnableInStop  = false, 		// MCGIRCLK disable in STOP mode.
        .ircs               = kMcgIrcFast, 	// Select IRC4M.
        .fcrdiv             = 0,    		// FCRDIV 0. Divide Factor is 1

        .frdiv   = 4,						// Divide Factor is 4 (128) (de todas maneras no afecta porque no usamos FLL)
        .drs     = kMcgDcoRangeSelMid,  	// mid frequency range (idem anterior)
        .dmx32   = kMcgDmx32Default,    	// DCO has a default range of 25% (idem anterior)

        .pll0EnableInFllMode = true,  		// PLL0 enable in FLL mode
        .pll0EnableInStop  = true,  		// PLL0 enable in STOP mode
        .prdiv0            = 0b11,			// divide factor 4 (Cristal 8Mhz / 4 * 24)
        .vdiv0             = 0b00,			// multiply factor 24
    },
    .simConfig =
    {
        .pllFllSel = kClockPllFllSelPll,    // PLLFLLSEL select PLL.
        .er32kSrc  = kClockEr32kSrcLpo,     // ERCLK32K selection, use LPO.
        .outdiv1   = 0b0000,				// Divide-by-1.
        .outdiv4   = 0b001,					// Divide-by-2.
    },
    .oscerConfig =
    {
        .enable       = true,  				// OSCERCLK enable.
        .enableInStop = true, 				// OSCERCLK enable in STOP mode.
    }
};

/*==================[internal functions declaration]=========================*/

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/
bool freeFalling;
bool calcAcMax;
bool titilar;
bool contar10s;
int16_t cuenta10=10000;
int16_t frecp=500;
int16_t acc=0;
int16_t accMax=0;
int8_t contar5ms=5;
bool cayo5ms=false;
/*==================[internal functions definition]==========================*/
void PIT_Init(void)
{
    uint32_t frecPerif;

    SIM_HAL_EnableClock(SIM, kSimClockGatePit0);
    PIT_HAL_Enable(PIT);
    CLOCK_SYS_GetFreq(kBusClock, &frecPerif);
    PIT_HAL_SetTimerPeriodByCount(PIT, 1, frecPerif/1000);
    PIT_HAL_SetIntCmd(PIT, 1, true);
    PIT_HAL_SetTimerRunInDebugCmd(PIT, false);
    PIT_HAL_StartTimer(PIT, 1);
    NVIC_ClearPendingIRQ(PIT_IRQn);
    NVIC_EnableIRQ(PIT_IRQn);

}

void InitOsc0(void)
{
    // OSC0 configuration.
    osc_user_config_t osc0Config =
    {
        .freq                = OSC0_XTAL_FREQ,
        .hgo                 = kOscGainLow,
        .range               = kOscRangeVeryHigh,
        .erefs               = kOscSrcOsc,
        .enableCapacitor2p   = false,
        .enableCapacitor4p   = false,
        .enableCapacitor8p   = false,
        .enableCapacitor16p  = false,
    };

    /* Setup board clock source. */
    // Setup OSC0 if used.
    // Configure OSC0 pin mux.
    PORT_HAL_SetMuxMode(EXTAL0_PORT, EXTAL0_PIN, EXTAL0_PINMUX);
    PORT_HAL_SetMuxMode(XTAL0_PORT, XTAL0_PIN, XTAL0_PINMUX);

    CLOCK_SYS_OscInit(0U, &osc0Config);
}

/* Initialize clock. */
void ClockInit(void)
{
	uint32_t frecPerif;
	/* Set allowed power mode, allow all. */
    SMC_HAL_SetProtection(SMC, kAllowPowerModeAll);
    CLOCK_SYS_GetFreq(kBusClock, &frecPerif);
    InitOsc0();
    SysTick_Config(frecPerif/10);

    CLOCK_SYS_SetConfiguration(&clockConfigRun);
}


void mefCaida(void){
	static estMefCaida_enum estMefCaida = EST_MEF_CAIDA_INICIALIZACION;
	uint8_t BufLCD[6];
	switch (estMefCaida){

	case EST_MEF_CAIDA_INICIALIZACION:
		sprintf((char *)BufLCD,"%4d",cero);
		vfnLCD_Write_Msg(BufLCD);
		estMefCaida=EST_MEF_CAIDA_ESPERANDO_INTERRUPCION;
		mma8451_FF_MT_Enable();
		CLOCK_SYS_SetConfiguration(&clockConfigVlpr);
	break;

	case EST_MEF_CAIDA_ESPERANDO_INTERRUPCION:
		if (freeFalling){
			CLOCK_SYS_SetConfiguration(&clockConfigRun);
			mma8451_DataReadyEnable();
			estMefCaida=EST_MEF_CAIDA_CAYENDO;
		}
	break;

	case EST_MEF_CAIDA_CAYENDO:
		calcAcMax=true;
		titilar=true;
		if(cayo5ms){
			calcAcMax=false;
			sprintf((char *)BufLCD,"%4d",accMax);
			vfnLCD_Write_Msg(BufLCD);
			estMefCaida=EST_MEF_CAIDA_CAYO;
		}
	break;

	case EST_MEF_CAIDA_CAYO:
		contar10s=true;
		if(!cuenta10 || key_getPressEv(BOARD_SW_ID_1)){
			titilar=false;
			contar10s=false;
			cuenta10=10000;
			acc=0;
			cayo5ms=false;
			//calcAcMax=false;
			sprintf((char *)BufLCD,"%4d",acc);
			vfnLCD_Write_Msg(BufLCD);
			estMefCaida=EST_MEF_CAIDA_ESPERANDO_INTERRUPCION;
			mma8451_FF_MT_Enable();
			CLOCK_SYS_SetConfiguration(&clockConfigVlpr);
		}
	break;

	}
}

void mefLed(void){
	static estMefLed_enum estMefLed = EST_MEF_LED_ESPERANDO_ORDEN;

	switch(estMefLed){

	case EST_MEF_LED_ESPERANDO_ORDEN:

		if(titilar&&!frecp)
			estMefLed=EST_MEF_LED_TOOGLE;
		if(!titilar)
			board_setLed(BOARD_LED_ID_ROJO, BOARD_LED_MSG_OFF);
	break;

	case EST_MEF_LED_TOOGLE:
		board_setLed(BOARD_LED_ID_ROJO, BOARD_LED_MSG_TOGGLE);
		frecp=500;
		estMefLed=EST_MEF_LED_ESPERANDO_ORDEN;
	break;

	}
}

int16_t MaxAcc(void){
	if(calcAcMax){
		if(sqrt(mma8451_getAcc())>acc)
			acc=sqrt(mma8451_getAcc());
	}
	return acc;

}
/*==================[external functions definition]==========================*/

int main(void)
{


    // Enable clock for PORTs, setup board clock source, config pin
    board_init();

    ClockInit();

    PIT_Init();

    key_init();

    mma8451_setDataRate(DR_12p5hz);


    while (1)
    {

    	accMax=MaxAcc();
    	freeFalling = mma8451_fallingDetection();

    	mefCaida();
    	mefLed();

   }
}


void PIT_IRQHandler(void)
{
    PIT_HAL_ClearIntFlag(PIT, 1);

    key_periodicTask1ms();

    if(titilar){
    	if(mma8451_getAcc()>9400)
    		contar5ms--;
    	else
    		contar5ms=5;
    	if (contar5ms==0)
    		cayo5ms=true;
    	else
    		cayo5ms=false;
    	frecp--;
    }

    if(contar10s)
    	cuenta10--;

}

void SysTick_Handler(void){
	NVIC_GetPendingIRQ(SysTick_IRQn);
	board_setLed(BOARD_LED_ID_VERDE, BOARD_LED_MSG_TOGGLE);
}


/*==================[end of file]============================================*/
















