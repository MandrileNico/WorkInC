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
	EST_MEF_RESPUESTA_ESPERANDO_OK = 0,
	EST_MEF_RESPUESTA_VALIDAR_GRUPO,
	EST_MEF_RESPUESTA_DETECTAR_PERIFERICO,
	EST_MEF_RESPUESTA_LED_ROJO,
	EST_MEF_RESPUESTA_ACCION_LR,
	EST_MEF_RESPUESTA_LED_VERDE,
	EST_MEF_RESPUESTA_ACCION_LV,
	EST_MEF_RESPUESTA_SW1,
	EST_MEF_RESPUESTA_SW2,
	EST_MEF_RESPUESTA_ACELEROMETRO,
	EST_MEF_RESPUESTA_ACELEROMETRO_RESPUESTA,

}estMefRespuesta_enum;

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
void mefRespueta(void);




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
static bool procesar;
static bool datoValido;
static bool datoNoValido;
static int32_t acc;

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

	/* Set allowed power mode, allow all. */
    SMC_HAL_SetProtection(SMC, kAllowPowerModeAll);
    InitOsc0();
    CLOCK_SYS_SetConfiguration(&clockConfigRun);
}

void mefRespuesta(void){
	static estMefRespuesta_enum estMefRespuesta = EST_MEF_RESPUESTA_ESPERANDO_OK;

	bool grupoOK, grupoNoOK;
	bool LedRojo, LedVerde, SW1, SW2, Acc, cualquiera;

	switch (estMefRespuesta){

	case EST_MEF_RESPUESTA_ESPERANDO_OK:
		if (procesar){
			datoNoValido=false;
			datoValido=false;
			estMefRespuesta=EST_MEF_RESPUESTA_DETECTAR_PERIFERICO;
		}
	break;


	case EST_MEF_RESPUESTA_DETECTAR_PERIFERICO:
		//Ver el 11 en el buffer con los ifs anidados
		if (grupoOK)
			estMefRespuesta=EST_MEF_RESPUESTA_DETECTAR_PERIFERICO;
		if(grupoNoOK){
			datoNoValido=true;
			estMefRespuesta=EST_MEF_RESPUESTA_ESPERANDO_OK;
		}
	break;

	case EST_MEF_RESPUESTA_DETECTAR_PERIFERICO:
		//Ver LedRojo, LedVerde, SW1, SW2 y Acc, cualquiera
		if(LedRojo)
			estMefRespuesta=EST_MEF_RESPUESTA_LED_ROJO;
		if(LedVerde)
			estMefRespuesta=EST_MEF_RESPUESTA_LED_VERDE;
		if(SW1)
			estMefRespuesta=EST_MEF_RESPUESTA_SW1;
		if(SW2)
			estMefRespuesta=EST_MEF_RESPUESTA_SW2;
		if(Acc){
			mma8451_DataReadyEnable();
			estMefRespuesta=EST_MEF_RESPUESTA_ACELEROMETRO;
		}
		if (cualquiera){
			datoNoValido=true;
			estMefRespuesta=EST_MEF_RESPUESTA_ESPERANDO_OK;
		}
	break;

	case EST_MEF_RESPUESTA_LED_VERDE:
		//ver E,A,T,cualquiera
		if (E){
			board_setLed(BOARD_LED_ID_VERDE, BOARD_LED_MSG_ON);
			estMefRespuesta=EST_MEF_RESPUESTA_ACCION_LV;
		}
		if (A){
			board_setLed(BOARD_LED_ID_VERDE, BOARD_LED_MSG_OFF);
			estMefRespuesta=EST_MEF_RESPUESTA_ACCION_LV;
		}
		if (T){
			board_setLed(BOARD_LED_ID_VERDE, BOARD_LED_MSG_TOGGLE);
			estMefRespuesta=EST_MEF_RESPUESTA_ACCION_LV;
		}
		if (cualquiera)
			estMefRespuesta=EST_MEF_RESPUESTA_ESPERANDO_OK;
	break;

	case EST_MEF_RESPUESTA_ACCION_LV:
		//mandar respuesta
		datoValido=true;
		estMefRespuesta=EST_MEF_RESPUESTA_ESPERANDO_OK;
	break;

	case EST_MEF_RESPUESTA_LED_ROJO:
			//ver E,A,T,cualquiera
			if (E){
				board_setLed(BOARD_LED_ID_ROJO, BOARD_LED_MSG_ON);
				estMefRespuesta=EST_MEF_RESPUESTA_ACCION_LR;
			}
			if (A){
				board_setLed(BOARD_LED_ID_ROJO, BOARD_LED_MSG_OFF);
				estMefRespuesta=EST_MEF_RESPUESTA_ACCION_LR;
			}
			if (T){
				board_setLed(BOARD_LED_ID_ROJO, BOARD_LED_MSG_TOGGLE);
				estMefRespuesta=EST_MEF_RESPUESTA_ACCION_LR;
			}
			if (cualquiera)
				estMefRespuesta=EST_MEF_RESPUESTA_ESPERANDO_OK;
		break;

		case EST_MEF_RESPUESTA_ACCION_LR:
			//mandar respuesta
			datoValido=true;
			estMefRespuesta=EST_MEF_RESPUESTA_ESPERANDO_OK;
		break;

		case EST_MEF_RESPUESTA_SW1:
			//leer estado y mandar respuesta
			datoValido=true;
			estMefRespuesta=EST_MEF_RESPUESTA_ESPERANDO_OK;
		break;

		case EST_MEF_RESPUESTA_SW2:
			//leer estado y mandar respuesta
			datoValido=true;
			estMefRespuesta=EST_MEF_RESPUESTA_ESPERANDO_OK;
		break;

		case EST_MEF_RESPUESTA_ACELEROMETRO:
			acc=(mma8451_getAcc())/(mma8451_getAcc());
			estMefRespuesta=EST_MEF_RESPUESTA_ACELEROMETRO_RESPUESTA;
		break;

		case EST_MEF_RESPUESTA_ACELEROMETRO_RESPUESTA:
			//armar y devolver respuesta
			mma8451_DataReadyDisable();
			estMefRespuesta=EST_MEF_RESPUESTA_ESPERANDO_OK;

	}

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




/*==================[end of file]============================================*/
















