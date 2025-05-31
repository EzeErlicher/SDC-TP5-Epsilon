#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define GPIO_CHIP "/dev/gpiochip0"

// Pines GPIO (números Broadcom GPIO)
#define GPIO_OUT1 23  // Salida 1Hz
#define GPIO_OUT2 24  // Salida 2Hz

void delay_ms(int ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
}
int main(int argc, char* argv[]) {
    struct gpiod_chip *chip;
    struct gpiod_line *line_out1, *line_out2;
    int ret;
    int out1_state = 0, out2_state = 0;
    int counter_1Hz = 0, counter_2Hz = 0;
    // Abrir el gpiochip0
    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("Error al abrir gpiochip");
        return 1;
    }
    // Obtener líneas
    line_out1 = gpiod_chip_get_line(chip, GPIO_OUT1);
    line_out2 = gpiod_chip_get_line(chip, GPIO_OUT2);
    if (!line_out1 || !line_out2) {
        fprintf(stderr, "Error obteniendo líneas GPIO\n");
        gpiod_chip_close(chip);
        return 1;
    }
    // Configurar líneas de salida
    ret = gpiod_line_request_output(line_out1, "gen_signal", 0);
    if (ret < 0) { perror("Error request output line_out1"); return 1; }
    ret = gpiod_line_request_output(line_out2, "gen_signal", 0);
    if (ret < 0) { perror("Error request output line_out2"); return 1; }
    printf("Iniciando loop principal...\n");

    while (1) {
        // Toggle salida 1 cada X ms
        if (counter_1Hz >= atoi(argv[1])) {
            out1_state = !out1_state;
            gpiod_line_set_value(line_out1, out1_state);
            printf("GPIO %d set a %d\n", GPIO_OUT1, out1_state);
            counter_1Hz = 0;
        }
        // Toggle salida 2 cada Y ms
        if (counter_2Hz >= atoi(argv[2])) {
            out2_state = !out2_state;
            gpiod_line_set_value(line_out2, out2_state);
            printf("GPIO %d set a %d\n", GPIO_OUT2, out2_state);
            counter_2Hz = 0;
        }
        delay_ms(10);
        counter_1Hz += 10;
        counter_2Hz += 10;
    }
    // Nunca llega acá pero por limpieza
    gpiod_chip_close(chip);
    return 0;
}
