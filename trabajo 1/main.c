/*==================[inclusions]=============================================*/

// Standard C Included Files
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// SDK Included Files
#include "fsl_sim_hal.h"
#include "fsl_mcg_hal.h"
#include "fsl_tpm_hal.h"
#include "fsl_pit_hal.h"
#include "fsl_clock_manager.h"
#include "fsl_lpsci_hal.h"


// Project Included Files
#include "board.h"
#include "key.h"

/*==================[macros and definitions]=================================*/
typedef enum
{
	EST_MEF_LED_INICIALIZACION = 0,
	EST_MEF_LED_ESPERANDO_EV,
	EST_MEF_LED_ESPERANDO_EV1,
	EST_MEF_LED_ESPERANDO_EV2,
	EST_MEF_LED_ESPERANDO_EV3,
}estMefLedI_enum;
typedef enum
{
	EST_MEF_LED_IF = 0,
	EST_MEF_LED_ESPERANDO_EVF,
	EST_MEF_LED_ESPERANDO_EVF1,
	EST_MEF_LED_ESPERANDO_EVF2,
	EST_MEF_LED_ESPERANDO_EVF3,
}estMefLedF_enum;
typedef enum
{
	EST_MEF_LED_INI = 0,
	EST_MEF_LED_PREN,
	EST_MEF_LED_APAG,
}mefLedParpadeo_enum;

#define CHANNEL_PWM				2

/*==================[internal data declaration]==============================*/
static int32_t ValDuty = 0;
static int32_t Duty100porciento;
static int32_t DutyStep, Inten, tim, cuenta ;
/*==================[internal functions declaration]=========================*/
void PIT_Init(void);

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

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

void TPM_Init(void)
{
	uint32_t frecPerif;
	uint32_t channel;

	tpm_pwm_param_t ParamPwm;

	ParamPwm.mode = kTpmEdgeAlignedPWM;
	ParamPwm.edgeMode = kTpmLowTrue;

	// Habilito clock del periferico
	SIM_HAL_EnableClock(SIM, kSimClockGateTpm0);

	// Detengo cuenta del Timer
	TPM_HAL_SetClockMode(TPM0, kTpmClockSourceNoneClk);

	// Reseteo el timer
	TPM_HAL_Reset(TPM0, 0);

	// Clock prescaler = 1
	TPM_HAL_SetClockDiv(TPM0, kTpmDividedBy1);

	// Borro bandera de overflow
	TPM_HAL_ClearTimerOverflowFlag(TPM0);

	// Deshabilito todos los canales
	for (channel = 0; channel < FSL_FEATURE_TPM_CHANNEL_COUNT ; channel++)
	{
		TPM_HAL_DisableChn(TPM0, channel);
	}

	// NO Activo interupcion ante bandera de OverFlow
	TPM_HAL_DisableTimerOverflowInt(TPM0);

	// Se habilita IRC y se elige Frec alta
    CLOCK_HAL_SetInternalRefClkEnableCmd(MCG, true);
    CLOCK_HAL_SetInternalRefClkMode(MCG, kMcgIrcFast);

	// Elijo la fuente de clock para el TPM
	CLOCK_HAL_SetTpmSrc(SIM, 0, kClockTpmSrcMcgIrClk);

	// Obtengo Frecuencia IR MCG con la que alimento el TPM
	CLOCK_SYS_GetFreq(kMcgIrClock,&frecPerif);

	// Frec. de PWM 1KHz -- Periodo 1mseg
	Duty100porciento = frecPerif / 1000;
	DutyStep = Duty100porciento / 100;

	// Valor de cuenta max. -- Frec de PWM
	TPM_HAL_SetMod(TPM0, Duty100porciento);

	// Seteo el duty cycle inicial
	TPM_HAL_SetChnCountVal(TPM0, CHANNEL_PWM, ValDuty);

	// Habilito el canal PWM
	TPM_HAL_EnablePwmMode(TPM0, &ParamPwm, CHANNEL_PWM);

	// Habilito el Timer -- Fuente de clock interna
	TPM_HAL_SetClockMode(TPM0, kTpmClockSourceModuleClk);

	board_ledOutPWM(BOARD_LED_ID_ROJO);
}
// control de intensidad a traves Pwm
void mefLedI(void)
{
    static estMefLedI_enum estMefLedI = EST_MEF_LED_INICIALIZACION;

    switch (estMefLedI)
    {
        case EST_MEF_LED_INICIALIZACION:
        		Inten=Duty100porciento*0.15;
        		estMefLedI = EST_MEF_LED_ESPERANDO_EV;
        break;

        case EST_MEF_LED_ESPERANDO_EV:
            if (key_getPressEv(BOARD_SW_ID_3))
            {
            	Inten= Duty100porciento*0.4;
            	estMefLedI = EST_MEF_LED_ESPERANDO_EV1;
            }
        break;

        case EST_MEF_LED_ESPERANDO_EV1:
            if (key_getPressEv(BOARD_SW_ID_3))
            {
            	Inten= Duty100porciento*0.7;
                estMefLedI = EST_MEF_LED_ESPERANDO_EV2;
            }
            break;

        case EST_MEF_LED_ESPERANDO_EV2:
            if (key_getPressEv(BOARD_SW_ID_3))
            {
            	Inten=Duty100porciento;
                estMefLedI = EST_MEF_LED_ESPERANDO_EV3;
            }
        break;

        case EST_MEF_LED_ESPERANDO_EV3:
            if (key_getPressEv(BOARD_SW_ID_3))
            {
            	Inten=Duty100porciento*0.15;
                estMefLedI = EST_MEF_LED_ESPERANDO_EV;
            }
        break;

        default:
        	 estMefLedI = EST_MEF_LED_INICIALIZACION;
        break;

    }
}
// control de la frecuencia de parpadeo
void mefLedF(void)
{
        static estMefLedF_enum estMefLedF = EST_MEF_LED_IF;

        switch (estMefLedF)
        {
            case EST_MEF_LED_IF:
                	cuenta=900;
                	estMefLedF = EST_MEF_LED_ESPERANDO_EVF;
            break;

            case EST_MEF_LED_ESPERANDO_EVF:
                if (key_getPressEv(BOARD_SW_ID_1))
                {
                	cuenta=700;
                	estMefLedF = EST_MEF_LED_ESPERANDO_EVF1;
                }
            break;

            case EST_MEF_LED_ESPERANDO_EVF1:
                if (key_getPressEv(BOARD_SW_ID_1))
                {
                	cuenta=400;
                	estMefLedF = EST_MEF_LED_ESPERANDO_EVF2;
                }
            break;

            case EST_MEF_LED_ESPERANDO_EVF2:
                if (key_getPressEv(BOARD_SW_ID_1))
                {
                	estMefLedF = EST_MEF_LED_ESPERANDO_EVF3;
                	cuenta=100;
                }
            break;

            case EST_MEF_LED_ESPERANDO_EVF3:
                if (key_getPressEv(BOARD_SW_ID_1))
                {
                	estMefLedF = EST_MEF_LED_ESPERANDO_EVF;
                	cuenta=900;
                }
            break;

            default:
            	estMefLedF = EST_MEF_LED_IF;
            break;


        }
    }
// mef del control del prendido y apagado del led
void mefLedParpadeo()
{
	static mefLedParpadeo_enum mefLedParpadeo = EST_MEF_LED_INI;
	switch (mefLedParpadeo)
	{
		case EST_MEF_LED_INI:
			tim=0;
			TPM_HAL_SetChnCountVal(TPM0, CHANNEL_PWM, 0);
			mefLedParpadeo = EST_MEF_LED_PREN;
	    break;
		case EST_MEF_LED_PREN:
			if(tim==0)
			{
			tim=cuenta;
			TPM_HAL_SetChnCountVal(TPM0, CHANNEL_PWM, Inten);
			mefLedParpadeo = EST_MEF_LED_APAG;
			}
		break;

		case EST_MEF_LED_APAG:
			if(tim==0)
			{
				tim=cuenta;
				TPM_HAL_SetChnCountVal(TPM0, CHANNEL_PWM, 0);
				mefLedParpadeo = EST_MEF_LED_PREN;
			}
		break;

		default:
			mefLedParpadeo = EST_MEF_LED_INI;


	}
}
/*==================[external functions definition]==========================*/

int main(void)
{
	// Se inicializan funciones de la placa
	board_init();

	// Inicializa keyboard
	key_init();

	// Se inicializa el timer PIT
	PIT_Init();

	TPM_Init();

    while(1)
    {
    	mefLedI();
    	mefLedF();
    	mefLedParpadeo();
    }
}

void PIT_IRQHandler(void)
{
    PIT_HAL_ClearIntFlag(PIT, 1);

    key_periodicTask1ms();
    if (tim)
       {
           tim--;
       }
}

/*==================[end of file]============================================*/
