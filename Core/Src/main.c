/* Alarmni sistem – EXTI + UART RX DMA  --------------------------------- */
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define RX_DMA_BUFSIZE 256
#define DEBOUNCE_MS    250   /* minimalni čas med veljavnimi pritiski */

/* --------------------- LED ------------------------------------------- */
#define LED1_PORT GPIOE
#define LED1_PIN  GPIO_PIN_3
#define LED2_PORT GPIOH
#define LED2_PIN  GPIO_PIN_15
#define LED3_PORT GPIOB
#define LED3_PIN  GPIO_PIN_4
#define LED4_PORT GPIOB
#define LED4_PIN  GPIO_PIN_15   /* zelena */

/* --------------------- Gumbi (PI2, PE6, PA8, PK1) -------------------- */
#define BTN1_PIN  GPIO_PIN_2
#define BTN1_PORT GPIOI
#define BTN2_PIN  GPIO_PIN_6
#define BTN2_PORT GPIOE
#define BTN3_PIN  GPIO_PIN_8
#define BTN3_PORT GPIOA
#define BTN4_PIN  GPIO_PIN_1
#define BTN4_PORT GPIOK

/* --------------------- Ultrazvok ------------------------------------- */
#define US_TRIG_PORT  TRIGF_GPIO_Port
#define US_TRIG_PIN   TRIGF_Pin
#define US_ECHO_PORT  ECHO_GPIO_Port
#define US_ECHO_PIN   ECHO_Pin

typedef enum { MONITOR, COUNTDOWN, ALARM, DISARMED } State_e;

/* --------------------- HAL handlers ---------------------------------- */
UART_HandleTypeDef huart3;
DMA_HandleTypeDef  hdma_usart3_tx;
DMA_HandleTypeDef  hdma_usart3_rx;

/* --------------------- Globalno stanje ------------------------------- */
static uint8_t  rxBuf[RX_DMA_BUFSIZE];
static uint32_t pressCnt[4]   = {0};
static uint32_t lastPressMs[4]= {0};      /* ↓ debounce časovni žig */
static uint8_t  attemptCnt    = 0;
static bool     ledState[4]   = {false};
static State_e  state         = MONITOR;
static uint32_t secLeft, lastSec, lastBlink, disarmUntil;
static bool     blink;

/* ===================== DWT mikrosekunde ============================== */
static inline void DWT_Init(void){
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}
static inline uint32_t micros(void){
    return DWT->CYCCNT / (SystemCoreClock/1000000);
}

/* ===================== printf (blokirajoče TX) ======================= */
int _write(int fd,char *ptr,int len){
    HAL_UART_Transmit(&huart3,(uint8_t*)ptr,len,HAL_MAX_DELAY);
    return len;
}

/* ===================== LED helper ==================================== */
static void set_led(uint8_t id,bool on){
    if(id<1||id>4||ledState[id-1]==on) return;
    GPIO_TypeDef *p; uint16_t pin;
    switch(id){
      case 1:p=LED1_PORT;pin=LED1_PIN;break;
      case 2:p=LED2_PORT;pin=LED2_PIN;break;
      case 3:p=LED3_PORT;pin=LED3_PIN;break;
      default:p=LED4_PORT;pin=LED4_PIN;
    }
    ledState[id-1]=on;
    HAL_GPIO_WritePin(p,pin,on?GPIO_PIN_SET:GPIO_PIN_RESET);
    printf("LED%u:%s\r\n",id,on?"ON":"OFF");
}

/* ===================== Buzzer ======================================== */
static void beep(uint32_t ms){
    for(uint32_t i=0;i<ms;i++){
        HAL_GPIO_TogglePin(GPIOG,GPIO_PIN_3);
        HAL_Delay(1);
    }
    HAL_GPIO_WritePin(GPIOG,GPIO_PIN_3,GPIO_PIN_RESET);
}

/* ===================== Ultrazvok ===================================== */
static uint32_t us_read_cm(void){
    uint32_t t0,t1,t2;
    HAL_GPIO_WritePin(US_TRIG_PORT,US_TRIG_PIN,GPIO_PIN_SET);
    t0=micros(); while(micros()-t0<10);
    HAL_GPIO_WritePin(US_TRIG_PORT,US_TRIG_PIN,GPIO_PIN_RESET);
    t0=micros();
    while(!HAL_GPIO_ReadPin(US_ECHO_PORT,US_ECHO_PIN))
        if(micros()-t0>30000) return UINT32_MAX;
    t1=micros();
    while(HAL_GPIO_ReadPin(US_ECHO_PORT,US_ECHO_PIN))
        if(micros()-t1>30000) break;
    t2=micros();
    return (t2-t1)/58;
}


static void print_btn(void){
    printf("G1:%lu G2:%lu G3:%lu G4:%lu\r\n",
           (unsigned long)pressCnt[0],(unsigned long)pressCnt[1],
           (unsigned long)pressCnt[2],(unsigned long)pressCnt[3]);
}
static void register_press(uint8_t idx){
    uint32_t now = HAL_GetTick();
    if (now - lastPressMs[idx] < DEBOUNCE_MS) return; /* odbij odboj */
    lastPressMs[idx] = now;

    pressCnt[idx]++; attemptCnt++;
    print_btn();
}
void HAL_GPIO_EXTI_Callback(uint16_t pin){
    if      (pin == BTN1_PIN) register_press(0);
    else if (pin == BTN2_PIN) register_press(1);
    else if (pin == BTN3_PIN) register_press(2);
    else if (pin == BTN4_PIN) register_press(3);
}


static void reset_btn(void){
    memset(pressCnt,0,sizeof pressCnt);
    memset(lastPressMs,0,sizeof lastPressMs);
    attemptCnt = 0;
}


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *hu,uint16_t sz){
    static char line[64]; static uint8_t pos=0;
    for(uint16_t i=0;i<sz;i++){
        char c=rxBuf[i];
        if(c=='\r'||c=='\n'){
            line[pos]=0;
            if     (!strcmp(line,"BTN1")) register_press(0);
            else if(!strcmp(line,"BTN2")) register_press(1);
            else if(!strcmp(line,"BTN3")) register_press(2);
            else if(!strcmp(line,"BTN4")) register_press(3);
            pos=0;
        }else if(pos<63 && c>=32 && c<127) line[pos++]=c;
    }
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3,rxBuf,RX_DMA_BUFSIZE);
}


void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART3_UART_Init(void);


int main(void){
    MPU_Config(); HAL_Init(); SystemClock_Config();
    MX_GPIO_Init(); MX_DMA_Init(); MX_USART3_UART_Init();
    DWT_Init();

    HAL_UARTEx_ReceiveToIdle_DMA(&huart3,rxBuf,RX_DMA_BUFSIZE);

    printf("Ready\r\n");

    uint32_t lastUs = HAL_GetTick(); lastSec = HAL_GetTick();

    while(1){
        uint32_t now = HAL_GetTick();

        /* -- dekodna logika (6 pritiskov) ----------------------------- */
        if ((state==COUNTDOWN||state==ALARM) && attemptCnt==6){
            if (pressCnt[0]==6 && !pressCnt[1] && !pressCnt[2] && !pressCnt[3]){
                state       = DISARMED;
                disarmUntil = now + 10000;
                set_led(1,false); set_led(2,false); set_led(3,false); set_led(4,true);
                HAL_GPIO_WritePin(GPIOG,GPIO_PIN_3,GPIO_PIN_RESET);
                printf("ALARM_DEAKTIVIRAN\r\n");
                reset_btn();
            } else {
                reset_btn();
                printf("NAPACNA_KODA\r\n");
            }
        }

        /* --------------------- FSM --------------------------------- */
        switch(state){

        case MONITOR:
            if (now - lastUs >= 500){
                lastUs = now;
                uint32_t d = us_read_cm();
                printf("US: %lu cm\r\n",(unsigned long)(d==UINT32_MAX?0:d));
                if (d < 100){
                    state       = COUNTDOWN;
                    secLeft     = 60;
                    lastSec     = now;
                    lastBlink   = now;
                    blink       = false;
                    printf(">>>Alarm:60s<<<\r\n");
                    printf("ALARM_SPROZEN\r\n");
                    beep(200);
                }
            }
            break;

        case COUNTDOWN:
            if (now - lastSec >= 1000){
                lastSec += 1000;
                --secLeft;
                printf("T-%02lu s\r\n",(unsigned long)secLeft);

                if      (secLeft > 55) { }
                else if (secLeft > 40 && (secLeft==45||secLeft==40)) beep(100);
                else if (secLeft > 20 && (secLeft % 5 == 0))         beep(100);
                else if (secLeft > 10 && (secLeft==20||secLeft==15)) beep(100);
                else if (secLeft <= 10)                              beep(100);

                if (secLeft == 0){
                    state       = ALARM;
                    lastBlink   = now;
                    printf("Alarmna agencija obveščena\r\n");
                    printf("ALARM_NOTIFIED\r\n");
                }
            }

            if (secLeft > 55){
                if (now - lastBlink >= 250){
                    lastBlink += 250;
                    blink = !blink;
                    for (int i=1;i<=3;++i) set_led(i,blink);
                    beep(50);
                }
            }
            else if (secLeft > 40){
                set_led(1,true); set_led(2,false); set_led(3,false);
            }
            else if (secLeft > 20){
                set_led(1,true); set_led(2,true ); set_led(3,false);
            }
            else if (secLeft > 10){
                set_led(1,true); set_led(2,true ); set_led(3,true );
            }
            else if (now - lastBlink >= 500){
                lastBlink += 500;
                blink = !blink;
                for (int i=1;i<=3;++i) set_led(i,blink);
            }
            break;

        case ALARM:
            if (now - lastBlink >= 500){
                lastBlink += 500;
                blink = !blink;
                for (int i=1;i<=3;++i) set_led(i,blink);
                beep(100);
            }
            break;

        case DISARMED:
            if (now >= disarmUntil){
                set_led(4,false);
                state  = MONITOR;
                lastUs = now;
            }
            break;
        }

        HAL_Delay(1);
    }
}


void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TRIGF_GPIO_Port, TRIGF_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PB4 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN1_Pin */
  GPIO_InitStruct.Pin = BTN1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PH15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : PE3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN2_Pin */
  GPIO_InitStruct.Pin = BTN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN3_Pin */
  GPIO_InitStruct.Pin = BTN3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PG3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN4_Pin */
  GPIO_InitStruct.Pin = BTN4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN4_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ECHO_Pin */
  GPIO_InitStruct.Pin = ECHO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ECHO_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : TRIGF_Pin */
  GPIO_InitStruct.Pin = TRIGF_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TRIGF_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(BTN4_EXTI_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(BTN4_EXTI_IRQn);

  HAL_NVIC_SetPriority(BTN1_EXTI_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(BTN1_EXTI_IRQn);

  HAL_NVIC_SetPriority(BTN2_EXTI_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(BTN2_EXTI_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
