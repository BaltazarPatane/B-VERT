#include <Arduino.h>
#include <TM1637Display.h>

#define TRIG 2
#define ECHO 3
#define DIO 8
#define CLK 9
#define BEEPA 11
#define BEEPB 12

#define FPS 20000UL
#define demasiado_lejos 3000UL // Tiempo mínimo que debe durar el pulso para considerar que la persona está en el aire, en microsegundos
#define dist_min 300UL // Tiempo mínimo que debe durar el pulso para considerar que no es un rebote interno, en microsegundos
#define muestras_para_salto 20
#define muestras_para_caida 2
#define muestras_para_informar muestras_para_caida
//#define cinco_cm_salto 300UL // Tiempo mínimo que debe durar el salto para evitar errores grandes, en microsegundos
//#define tiempo_presencia_ausencia 10000UL // Tiempo que debe pasar para considerar que la persona saltó o aterrizó, en microsegundos

typedef enum {
    REINICIANDO,
    LISTO,
    CONTANDO,
    TERMINADO
} maquina_estados;

const uint8_t JUMP[] = {
    SEG_D | SEG_E | SEG_B | SEG_C,
    SEG_D | SEG_E | SEG_B | SEG_C | SEG_F,
    SEG_A | SEG_E | SEG_B | SEG_C | SEG_F,
    SEG_A | SEG_E | SEG_B | SEG_G | SEG_F,
};

const uint8_t GUION[] = {SEG_G, SEG_G, SEG_G, SEG_G};

const uint8_t A[] = {SEG_A, SEG_A, SEG_A, SEG_A};
const uint8_t B[] = {SEG_B, SEG_B, SEG_B, SEG_B};
const uint8_t C[] = {SEG_C, SEG_C, SEG_C, SEG_C};
const uint8_t D[] = {SEG_D, SEG_D, SEG_D, SEG_D};
const uint8_t E[] = {SEG_E, SEG_E, SEG_E, SEG_E};
const uint8_t F[] = {SEG_F, SEG_F, SEG_F, SEG_F};

TM1637Display display(CLK, DIO);
volatile bool informar = true;
volatile bool esperando_eco = false;
volatile bool nueva_medicion = false;
volatile unsigned int chequeos = 0;
volatile unsigned int evil_chequeos_0 = 0;
volatile unsigned int evil_chequeos_999 = 0;
volatile unsigned long inicio_pulso = 0;
volatile unsigned long total_pulso = 0;
volatile unsigned long inicio_salto = 0;
volatile unsigned long inicio_ausencia = 0;
volatile unsigned long inicio_presencia = 0;
volatile unsigned int muestras_ausentes = 0;
volatile unsigned int muestras_presentes = 0;
volatile unsigned long total_salto = 0;
volatile unsigned long pulso_actual = 0;
volatile unsigned long inicio_pulso_actual = 0;
volatile unsigned long ultimo_disparo = 0;
volatile maquina_estados estado = REINICIANDO;

void carga() {
    for (int i = 1; i <= 3; i++) {
        display.setSegments(A); delay(100); display.setSegments(B); delay(100);
        display.setSegments(C); delay(100); display.setSegments(D); delay(100);
        display.setSegments(E); delay(100); display.setSegments(F); delay(100);
    }
    display.setSegments(GUION);
}

void inicializacion() {
    chequeos = 0;
    evil_chequeos_0 = 0;
    evil_chequeos_999 = 0;

    while (true) {
        // Envío pulso ultrasónico
        digitalWrite(TRIG, LOW); delayMicroseconds(2);
        digitalWrite(TRIG, HIGH); delayMicroseconds(10);
        digitalWrite(TRIG, LOW);

        // Espero respuesta del sensor o timeout
        unsigned long duracion = pulseIn(ECHO, HIGH, 30000L);

        float distancia = duracion * 0.034 / 2;
        Serial.println (distancia);

        if (duracion == 0) continue;

        // Si se encuentra a menos de 5cm no lo considero como una persona, sino como un error de medición
        if (duracion < dist_min) {
            chequeos = 0; evil_chequeos_999 = 0;
            if (evil_chequeos_0 > 2) {chequeos = 0; evil_chequeos_0 = 0;}
            else evil_chequeos_0++;
        }

        // Si se encuentra a más de 40cm no lo considero como una persona, sino como un error de medición
        else if (duracion > demasiado_lejos) {
            chequeos = 0; evil_chequeos_0 = 0;
            if (evil_chequeos_999 > 2) {chequeos = 0; evil_chequeos_999 = 0;}
            else evil_chequeos_999++;
        }

        // Si se encuentra entre 5cm y 40cm lo considero como una persona, y aumento el contador de chequeos
        else {chequeos++; evil_chequeos_0 = 0; evil_chequeos_999 = 0;}

        // Acumulo 3 chequeos por las dudas, para evitar falsos positivos por errores de medición
        if (chequeos > 0) break;
    }
}

void sonido_inicio() {
    for (int i = 0; i < 150; i++) {
        digitalWrite(BEEPA, HIGH); digitalWrite(BEEPB, LOW); delayMicroseconds(1500);
        digitalWrite(BEEPA, LOW); digitalWrite(BEEPB, HIGH); delayMicroseconds(1500);
    }
    digitalWrite(BEEPA, LOW); digitalWrite(BEEPB, LOW);
}

void sonido_final() {
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 100; i++) {
            digitalWrite(BEEPA, HIGH); digitalWrite(BEEPB, LOW); delayMicroseconds(1000);
            digitalWrite(BEEPA, LOW); digitalWrite(BEEPB, HIGH); delayMicroseconds(1000);
        }
        digitalWrite(BEEPA, LOW); digitalWrite(BEEPB, LOW); delay(100);
    }
}

void informar_resultado(int total) {
    for (int i = 1; i < 4; i++) {
        display.showNumberDecEx(total, false); delay(500);
        display.clear(); delay(500);
    }
    display.showNumberDecEx(total, false); delay(3000); display.clear();
}

void sensor() {
    unsigned long ahora = micros();

    if (digitalRead(ECHO) == HIGH) {
        inicio_pulso = ahora; // Si ECHO está en HIGH es porque se envío el pulso ultrasónico, y se inicia la cuenta del tiempo que dura el pulso
        esperando_eco = true;
    }
    else {
        total_pulso = ahora - inicio_pulso; // Si ECHO está en LOW es porque se recibió el pulso ultrasónico, y se calcula el tiempo que duró el pulso
        
        float distancia = total_pulso * 0.034 / 2;
        Serial.println (distancia);

        esperando_eco = false;
        nueva_medicion = true;
    }
}

void setup() {
    pinMode(TRIG, OUTPUT);
    pinMode(ECHO, INPUT);
    pinMode(BEEPA, OUTPUT);
    pinMode(BEEPB, OUTPUT);

    display.setBrightness(4);

    attachInterrupt(digitalPinToInterrupt(ECHO), sensor, CHANGE);

    Serial.begin(250000);
}

void loop() {

    if (estado == REINICIANDO) {
        carga();

        inicializacion();

        display.setSegments(JUMP);

        sonido_inicio();

        estado = LISTO;

        noInterrupts();
        nueva_medicion = false;
        total_pulso = 0;
        inicio_pulso = 0;
        esperando_eco = false;
        ultimo_disparo = 0;
        interrupts();
    }

    if (esperando_eco && (micros() - inicio_pulso > FPS)) {
        noInterrupts();
        total_pulso = FPS;
        esperando_eco = false;
        nueva_medicion = true;
        interrupts();
    }

    if (nueva_medicion) {

        noInterrupts();
        pulso_actual = total_pulso;
        inicio_pulso_actual  = inicio_pulso;
        nueva_medicion = false;
        interrupts();

        //--------------------------------------------------------------------------------

        if ((estado != LISTO && estado != CONTANDO) || pulso_actual < dist_min) {
            inicio_ausencia = 0;
            inicio_presencia = 0;
            muestras_ausentes = 0;
            muestras_presentes = 0;
        } else {

            // Si no está contando y el pulso es muy largo, se comprueba si fue un salto
            if (estado != CONTANDO){
                if (pulso_actual > demasiado_lejos) {

                    if (muestras_ausentes == 0) {
                        inicio_ausencia = inicio_pulso_actual;
                    }

                    muestras_ausentes++;

                    if (muestras_ausentes >= muestras_para_salto) {
                        inicio_salto = inicio_ausencia;
                        inicio_ausencia = 0;
                        muestras_ausentes = 0;
                        muestras_presentes = 0;
                        estado = CONTANDO;
                    }

                } else {
                    inicio_ausencia = 0;
                    muestras_ausentes = 0;
                }
            }

            // Si está contando y el pulso es corto, se comprueba si fue una caída
            else if (estado == CONTANDO){
                if (pulso_actual < demasiado_lejos) {

                    if (muestras_presentes == 0) {
                        inicio_presencia = inicio_pulso_actual;
                        total_salto = inicio_presencia - inicio_salto;
                    }

                    muestras_presentes++;

                    if (muestras_presentes >= muestras_para_informar) {
                        informar = false;
                    }

                    if (muestras_presentes >= muestras_para_caida) {
                        total_salto = inicio_presencia - inicio_salto;
                        inicio_presencia = 0;
                        muestras_presentes = 0;
                        estado = TERMINADO;
                    }

                } else {
                    inicio_presencia = 0;
                    muestras_presentes = 0;
                    informar = true;
                }
            }
        }
    }
    //--------------------------------------------------------------------------------

    if (estado == CONTANDO && informar) {
        float t = (inicio_pulso_actual - inicio_salto) / 1000000.0;
        float altura = t * t * 9.80665 * 0.125 * 100; // t^2 * g * 1/8 * 100, altura en cm
        if (altura < 0) altura = 0;
        display.showNumberDec((unsigned int)altura);
    }

    if (estado == TERMINADO) {
        sonido_final();

        float t = total_salto / 1000000.0;            // total en microsegundos, t en segundos
        float altura = t * t * 9.80665 * 0.125 * 100; // t^2 * g * 1/8 * 100, altura en cm
        if (altura < 0) altura = 0;
        informar_resultado((unsigned int)altura);

        estado = REINICIANDO;
        inicio_ausencia = 0;
        inicio_presencia = 0;
        muestras_ausentes = 0;
        muestras_presentes = 0;
        informar = true;
        nueva_medicion = false;
        esperando_eco = false;
        ultimo_disparo = 0;
    }

    if (!esperando_eco && (micros() - ultimo_disparo > FPS)) {

        ultimo_disparo = micros();

        noInterrupts();
        inicio_pulso = ultimo_disparo;
        esperando_eco = true;
        interrupts();

        digitalWrite(TRIG, LOW);
        delayMicroseconds(2);
        digitalWrite(TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(TRIG, LOW);
    }
}