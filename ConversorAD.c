#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "font.h"

// Definições dos pinos
#define LED_RED_PIN 13       // GPIO para LED Vermelho
#define LED_GREEN_PIN 11     // GPIO para LED Verde
#define LED_BLUE_PIN 12      // GPIO para LED Azul
#define JOYSTICK_X_PIN 26    // GPIO para eixo X do joystick
#define JOYSTICK_Y_PIN 27    // GPIO para eixo Y do joystick
#define JOYSTICK_PB 22       // GPIO para botão do joystick
#define BUTTON_A_PIN 5       // GPIO para botão A
#define I2C_SDA 14           // GPIO para I2C SDA
#define I2C_SCL 15           // GPIO para I2C SCL
#define I2C_PORT i2c1        // Porta I2C
#define SSD1306_ADDRESS 0x3C // Endereço I2C do display SSD1306
#define DEADZONE 20          // Define uma zona morta de 20 unidades do ADC

// Variáveis globais
ssd1306_t display;
bool green_led_state = false; // Estado do LED Verde
bool pwm_enabled = true;      // Estado do PWM (ligado/desligado)
uint8_t border_style = 0;     // Estilo da borda do display
uint16_t adc_value_x, adc_value_y; // Valores do joystick
uint8_t prev_square_x = 60, prev_square_y = 28; // Posição anterior do quadrado

// Variáveis de debounce separadas para cada pino
static uint32_t last_time_joystick = 0;
static uint32_t last_time_buttonA = 0;

// Função para inicializar o PWM
void init_pwm(void) {
    gpio_set_function(LED_RED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(LED_GREEN_PIN, GPIO_FUNC_PWM);
    gpio_set_function(LED_BLUE_PIN, GPIO_FUNC_PWM);

    uint slice_num_red = pwm_gpio_to_slice_num(LED_RED_PIN);
    uint slice_num_green = pwm_gpio_to_slice_num(LED_GREEN_PIN);
    uint slice_num_blue = pwm_gpio_to_slice_num(LED_BLUE_PIN);

    pwm_set_wrap(slice_num_red, 4095);
    pwm_set_wrap(slice_num_green, 4095);
    pwm_set_wrap(slice_num_blue, 4095);

    pwm_set_enabled(slice_num_red, true);
    pwm_set_enabled(slice_num_green, true);
    pwm_set_enabled(slice_num_blue, true);
}

// Função para atualizar o PWM dos LEDs com base nos valores do joystick
void update_pwm(uint16_t x_value, uint16_t y_value) {
    if (!pwm_enabled) return;

    // Controle do LED Vermelho (eixo X)
    uint16_t red_intensity = (abs(x_value - 2048) > DEADZONE) ? abs(x_value - 2048) * 2 : 0;
    pwm_set_gpio_level(LED_RED_PIN, red_intensity);

    // Controle do LED Azul (eixo Y)
    uint16_t blue_intensity = (abs(y_value - 2048) > DEADZONE) ? abs(y_value - 2048) * 2 : 0;
    pwm_set_gpio_level(LED_BLUE_PIN, blue_intensity);

    // Controle do LED Verde (botão do joystick)
    pwm_set_gpio_level(LED_GREEN_PIN, green_led_state ? 4095 : 0);
}

// Função de debounce com variável de tempo específica para cada pino
void debounce(uint gpio, uint32_t events, bool *state, uint32_t *last_time) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - *last_time < 200) return; // Debounce de 200ms
    *last_time = current_time;
    if (!gpio_get(gpio)) {  // Se o botão estiver pressionado (nível baixo)
        *state = !(*state); // Alterna o estado
    }
}

// Função de callback unificada para as interrupções dos GPIOs
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == JOYSTICK_PB) {
        // Trata a interrupção do botão do joystick:
        // - Alterna o estado do LED verde
        // - Altera o estilo da borda do display
        debounce(gpio, events, &green_led_state, &last_time_joystick);
        border_style = (border_style + 1) % 3; // Alterna entre 3 estilos de borda
        printf("Joystick: LED Verde: %d, Borda: %d\n", green_led_state, border_style);
    } else if (gpio == BUTTON_A_PIN) {
        // Trata a interrupção do botão A, alternando o estado do PWM
        debounce(gpio, events, &pwm_enabled, &last_time_buttonA);
        printf("Botão A: PWM habilitado: %d\n", pwm_enabled);
    }
}

// Função para desenhar a borda do display
void draw_border(void) {
    switch (border_style) {
        case 0:
            ssd1306_rect(&display, 0, 0, 128, 64, true, false); // Borda simples
            break;
        case 1:
            ssd1306_rect(&display, 2, 2, 124, 60, true, false); // Borda interna
            break;
        case 2:
            ssd1306_rect(&display, 4, 4, 120, 56, true, false); // Borda mais interna
            break;
    }
}

int main(void) {
    stdio_init_all();

    // Inicialização do ADC
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);

    // Inicialização do PWM
    init_pwm();

    // Inicialização do I2C e do display SSD1306
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init(&display, 128, 64, false, SSD1306_ADDRESS, I2C_PORT);
    ssd1306_config(&display);
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);

    // Configuração do pino do botão do joystick
    gpio_init(JOYSTICK_PB);
    gpio_set_dir(JOYSTICK_PB, GPIO_IN);
    gpio_pull_up(JOYSTICK_PB);

    // Configuração do pino do botão A
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);

    // Registra a função de callback unificada para as interrupções
    // Para o botão do joystick, usamos borda de descida (FALL)
    gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL, true, gpio_callback);
    // Para o botão A, configure também para borda de descida (FALL) para consistência
    gpio_set_irq_enabled(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true);

    while (true) {
        // Leitura dos valores do joystick
        adc_select_input(JOYSTICK_X_PIN - 26); // Seleciona o ADC para o eixo X
        adc_value_x = adc_read();
        adc_select_input(JOYSTICK_Y_PIN - 26); // Seleciona o ADC para o eixo Y
        adc_value_y = adc_read();

        // Atualização do PWM dos LEDs
        update_pwm(adc_value_x, adc_value_y);

        // Cálculo da posição do quadrado no display
        uint8_t square_x = (adc_value_y * 120) / 4096;        // Mapeia X para 0-120
        uint8_t square_y = ((4096 - adc_value_x) * 56) / 4096;  // Mapeia Y para 0-56

        // Apagar apenas a posição anterior do quadrado para evitar flickering
        ssd1306_rect(&display, prev_square_y, prev_square_x, 8, 8, false, true);

        // Desenha o novo quadrado
        ssd1306_rect(&display, square_y, square_x, 8, 8, true, true);
        printf("X: %d, Y: %d\n", adc_value_x, adc_value_y);  // Exibe no serial monitor

        // Atualiza a borda do display
        draw_border();
        ssd1306_send_data(&display); // Atualiza o display

        // Atualiza posição anterior do quadrado
        prev_square_x = square_x;
        prev_square_y = square_y;

        sleep_ms(10); // Pequeno delay para suavizar o movimento
    }
}
