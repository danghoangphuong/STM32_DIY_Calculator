/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include <i2c-lcd.h>
#include <Button_matrix.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#define EXPR_LEN 64	// max  len of expression

Matrix_Pin_Typedef rowS[4] = {
	{GPIOA, GPIO_PIN_15}, // ROW 1 - 4
	{GPIOB, GPIO_PIN_3},
	{GPIOB, GPIO_PIN_4},
	{GPIOB, GPIO_PIN_5},
};

Matrix_Pin_Typedef colS[5] = {
	{GPIOB, GPIO_PIN_12}, // col 1 - 4
	{GPIOB, GPIO_PIN_13},
	{GPIOB, GPIO_PIN_14},
	{GPIOB, GPIO_PIN_15},
	{GPIOA, GPIO_PIN_8},
};


Matrix_Typedef mMatrix = {
	.num_row = 4, 
	.num_col = 5,
	.row_pins = rowS,
	.col_pins = colS
};

const char* keymap[21] = {
	"",
	"%", "7", "8", "9", "/",
    "+/-", "4", "5", "6", "x",
    "C", "1", "2", "3", "-",
    "AC", "0", ".", "=", "+"
};

char buffer_display[32] = "";
char buffer_operand[32] = "";

float operand_A = 0;
float operand_B = 0;

char current_operand = '\0';

float result = 0;
char result_str[32];
float last_result = 0;
uint8_t is_continue_cal = 0;
float percentage = 0;

char buff_uart[40];

//====when pressing "="  ==========
char buffer_root_expr[EXPR_LEN]; // save root mathematical expression
uint8_t is_error = 0;
//========predence handle==============




void Print_uart(const char*format,...)
{
	for(uint8_t i=0; i<40; i++)
	{
		buff_uart[i] = 0;
	}
	va_list args;
	va_start(args, format);
	vsnprintf(buff_uart, sizeof(buff_uart), format, args);
	va_end(args);
	HAL_UART_Transmit(&huart1, (uint8_t*)buff_uart, 40, 1000);
}

void Send_message(char *str)
{
	lcd_goto_XY(1,0);
	lcd_send_string(str);
}

float Perform_plus(float num_a, float num_b)
{
	return num_a + num_b;
}

float Perform_minus(float num_a, float num_b)
{
	return num_a - num_b;
}

float Perform_multiply(float num_a, float num_b)
{
	return num_a * num_b;
}

float Perform_divide(float num_a, float num_b, uint8_t *is_error)
{
	if (num_b == 0.0f) 
	{
		*is_error = 1;
		return 0.0f;
	}
	*is_error = 0;
	float result = num_a / num_b;
	return result;
}   


float Perform_cal(float num_a, float num_b, char operand)
{
	switch(operand)
	{
		case '+':
			return num_a + num_b;
		case '-':
			return num_a - num_b;
		case 'x':
			return num_a * num_b;
		case '/':
			if(num_b != 0)
			{
				is_error = 0;
				return num_a/num_b;
			}
			else
			{
				is_error = 1;
				return 0;
			}
	}
	return 0;
}

uint8_t Compare_predence(char operand)
{
	if(operand == 'x' || operand == '/') return 2;
	if(operand == '+' || operand == '-') return 1;
	return 0;
}


float Predence_handle(const char *buffer_expr)
{
	float num_stack[32]; // save number to this stack 
	char operand_stack[32];
	float num = 0;
	int num_top = -1; // blank
	int operand_top = -1; // blank
	
	for(int i=0; buffer_expr[i] != '\0'; i++)
	{
		// Duyet root buffer 
		char c = buffer_expr[i];
		
		// neu c la so hoac dau .
		if((c >= '0' && c <= '9') || c == '.')
		{
			// doc het so, sau do luu vao temp (vd: 123.45)
			// gap toan tu (+ - x /) thi dung`
			char temp[16];
			int j = 0;
			while((buffer_expr[i] >= '0' && buffer_expr[i] <= '9') || buffer_expr[i] == '.')
			{
				// vd: 123.45 + 10 => temp = 123.45
				temp[j++] = buffer_expr[i++];
			}
			temp[j] = '\0';
			num = atof(temp); // convert to number
			num_stack[++num_top] = num; // add num to num_stack[0]
			i--;
		}
		else if(c == '+' || c == '-' || c == 'x' || c == '/')
		{
			// stack not blank && compare predence of top with current
			while(operand_top >= 0 && Compare_predence(operand_stack[operand_top]) >= Compare_predence(c))
			{
				float num_2 = num_stack[num_top--]; // newest number in number stack
				float num_1 = num_stack[num_top--];	// older (on the left) eg: [6,2] -> num_2 = 2, num_1 = 6
				char op = operand_stack[operand_top--]; // take higher predence operator
				num_stack[++num_top] = Perform_cal(num_1, num_2, op); // perform num_2 op num_1 and push result back to number stack
			}
			operand_stack[++operand_top] = c;
		}
	}
	while (operand_top >= 0) 
	{
        float num_2 = num_stack[num_top--];
        float num_1 = num_stack[num_top--];
        char op = operand_stack[operand_top--];
        num_stack[++num_top] = Perform_cal(num_1, num_2, op);
    }
    return num_stack[num_top];
}

void Format_number(double val, char *buf, int8_t precision)
{
    sprintf(buf, "%.*f", precision, val);
    char *p = buf + strlen(buf) - 1; // point to last element 
    // eg: 12345; p->5
    while(*p == '0' && p > buf) // delete zero (eg: 123.000 -> 123.)
    {
        *p = '\0';
        p--;
    }
    if(*p == '.')
    {
        *p = '\0';
    }
}

void Key_pressed_handle(const char *key)
{
	//===========number======================================
	if(strcmp(key, "0") == 0 || strcmp(key, "1") == 0 || 
	   strcmp(key, "2") == 0 || strcmp(key, "3") == 0 ||
	   strcmp(key, "4") == 0 || strcmp(key, "5") == 0 ||
	   strcmp(key, "6") == 0 || strcmp(key, "7") == 0 || 
	   strcmp(key, "8") == 0 || strcmp(key, "9") == 0 || strcmp(key, ".") == 0)
	{
		if(is_continue_cal)
		{
			strcpy(buffer_display, "");
			strcpy(buffer_operand, "");
			is_continue_cal = 0;
		}
		
		strcat(buffer_display, key);
		strcat(buffer_operand, key);
		strcat(buffer_root_expr, key); // concat to perform chain calculate
		
		lcd_goto_XY(0,0);
		lcd_send_string("               ");
		lcd_goto_XY(0,0);
		lcd_send_string(buffer_display);
	}
//============================operator========================================	
	else if(strcmp(key, "+") == 0 || strcmp(key, "-") == 0 || 
			strcmp(key, "x") == 0 || strcmp(key, "/") == 0 )
	{
		if(is_continue_cal) // continue calculating after press = key
		{
			operand_A = last_result;
			sprintf(buffer_display, "%g", operand_A);
			is_continue_cal = 0;
			strcpy(buffer_operand, "");
		}
		else 	// perform from scratch
		{
			if(strlen(buffer_operand) > 0)
			{
				float val = atof(buffer_operand);
				if(current_operand == '\0') // first time press operator
				{
					operand_A = val;
				}
				else // if operator is existed
				{
					operand_B = val;
				}
//				sprintf(buffer_display, "%g", operand_A);
				strcpy(buffer_operand, "");
			}
		}
		current_operand = key[0];
		strcat(buffer_display, key);
		strcat(buffer_root_expr, key);
		
		lcd_goto_XY(0,0);
		lcd_send_string("               ");
		lcd_goto_XY(0,0);
		lcd_send_string(buffer_display);
	}
	
//================== calculate Percentage=================
	else if(strcmp(key, "%")==0)
	{
		percentage = (float)atof(buffer_operand); 
		if (current_operand == '\0') 
		{
			operand_A = percentage / 100.0f;
			sprintf(buffer_operand, "%g", operand_A);
			sprintf(buffer_display, "%g", operand_A);
		} 
		else 
		{
			if (current_operand == '+' || current_operand == '-') 
			{
				operand_B = operand_A * (percentage / 100.0f);
			} 
			else 
			{
				operand_B = percentage / 100.0f;
			}

			sprintf(buffer_operand, "%g", operand_B);
			snprintf(buffer_display, sizeof(buffer_display), "%g%c%s", operand_A, current_operand, buffer_operand);
		}
		lcd_clear_display();
		lcd_goto_XY(0,0);
		Print_uart("buffer_display = [%s]\n", buffer_display);
		lcd_send_string(buffer_display);
	}
	
	else if(strcmp(key, "+/-") == 0)
	{
		
	}
//=======================calculate result ====================
	else if(strcmp(key, "=") == 0)
	{
		strcpy(buffer_root_expr, buffer_display);
		result = Predence_handle(buffer_root_expr); 
		
		last_result = result;
		is_continue_cal = 1;
		
		strcpy(buffer_operand, ""); // clear buffer
		
		lcd_goto_XY(0,0);
		lcd_send_string("               ");
		lcd_goto_XY(0,0);
		lcd_send_string(buffer_display);

		if(is_error == 1)
		{
			Send_message("Syntax ERROR");
		}
		else	
		{
			Format_number(result,result_str,12);
			lcd_goto_XY(1,0);
			lcd_send_string("               ");
			lcd_goto_XY(1,0);
			lcd_send_string(result_str);
		}
		current_operand = '\0';
	}
	
//	 clear handle
	else if(strcmp(key, "C") == 0)
	{
		uint8_t len_display = strlen(buffer_display);
		uint8_t len_operand = strlen(buffer_operand);
		if(len_display > 0) 
		{
			buffer_display[len_display-1] = '\0';
		}
		if(len_operand > 0) 
		{
			buffer_operand[len_operand-1] = '\0';
		}
		lcd_goto_XY(0,0);
		lcd_send_string("                "); // clear line 1
		lcd_goto_XY(0,0);
		lcd_send_string(buffer_display);
	}
	
	else if(strcmp(key, "AC")==0)
	{
		strcpy(buffer_operand, "");
		strcpy(buffer_display, "");
		strcpy(buffer_root_expr, "");
		
		operand_A = operand_B = last_result = 0;
		current_operand = '\0';
		lcd_clear_display();
	}
}


void Matrix_key_press_Callback(uint8_t key, KeyEvent_t key_event) // Callback
{
	if(key_event == KEY_PRESS)
	{
		if(key < 0 || key > 20) return;
		const char *key_pressed = keymap[key];
		Key_pressed_handle(key_pressed);
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	lcd_init();
	lcd_goto_XY(0,2);
	lcd_send_string("May tinh cua");
	lcd_goto_XY(1,2);
	lcd_send_string("Diep beo");
	HAL_Delay(2000);
	lcd_clear_display();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
	Matrix_handle(&mMatrix);
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ROW1_GPIO_Port, ROW1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, ROW2_Pin|ROW3_Pin|ROW4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : COL1_Pin COL2_Pin COL3_Pin COL4_Pin */
  GPIO_InitStruct.Pin = COL1_Pin|COL2_Pin|COL3_Pin|COL4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : COL5_Pin */
  GPIO_InitStruct.Pin = COL5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(COL5_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ROW1_Pin */
  GPIO_InitStruct.Pin = ROW1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ROW1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ROW2_Pin ROW3_Pin ROW4_Pin */
  GPIO_InitStruct.Pin = ROW2_Pin|ROW3_Pin|ROW4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
