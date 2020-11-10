//==================================================================
// Medidor de color modelando un Contador Universal Reversible
//==================================================================

#include <LiquidCrystal_I2C.h> 

// Pines de Módulo TCS3200
#define S0 8
#define S1 13
#define S2 12
#define S3 11
#define OUT 10
#define LUZ 9

// Filtros de color
#define NONE 0
#define RED 1
#define GREEN 2
#define BLUE 3

// Escalas de frecuencia
#define FREQ_0 0
#define FREQ_2 1
#define FREQ_20 2
#define FREQ_100 3

// Modos de salida
#define PLOT 0
#define NORMAL 1
#define LED 2
#define CALIBRAR 3

// Calibración FREQ_100 (Se ajusta minimos y luego máximos)
/* // 128
#define FREQ_R_MIN 2885.0 //negro
#define FREQ_G_MIN 3400.0
#define FREQ_B_MIN 3887.0

#define FREQ_R_MAX 8850.0 //blanco
#define FREQ_G_MAX 11550.0
#define FREQ_B_MAX 8990.0*/

// 64
#define FREQ_R_MIN 1255.0 //negro
#define FREQ_G_MIN 1735.0
#define FREQ_B_MIN 1985.0

//#define FREQ_R_MAX 4748.0 //blanco
#define FREQ_R_MAX 4748.0 //blanco
#define FREQ_G_MAX 5673.0
#define FREQ_B_MAX 4905.0


//==================================================================
// Definición de variables
//==================================================================

// Frecuencias de los fotodiodos
long freqRed = 0;
long freqGreen = 0;
long freqBlue = 0;

// Variables del timer
long inicioTimer = 0; // instante en milisegundos en que se inicia el timer
bool timerOn = false; // verdadero si el timer sigue activo

// Variables del CUR
int lectura;
long acumLect = 0;
long acumClock = 0;
int lectAnterior = 0;
float freqMedida = 0;
long deltaT;
int turno = 0;

// Colores
long colorR = 0;
long colorG = 0;
long colorB = 0;

// Creacion de lcd
LiquidCrystal_I2C lcd(0x27,16,2);

// Modo de multiplicador de frecuencia del sensor
void modoFreq(int modo) {
  switch (modo) {
    case FREQ_0:
      digitalWrite(S0, LOW);
      digitalWrite(S1, LOW);
      break;

    case FREQ_2:
      digitalWrite(S0, LOW);
      digitalWrite(S1, HIGH);
      break;

    case FREQ_20:
      digitalWrite(S0, HIGH);
      digitalWrite(S1, LOW);
      break;

    case FREQ_100:
      digitalWrite(S0, HIGH);
      digitalWrite(S1, HIGH);
      break;
  }
}

// Modo de filtro de color
void modoFiltro(int color) {
  switch (color) {
    case RED:
        digitalWrite(S2, LOW);
        digitalWrite(S3, LOW);
        break;
  
      case GREEN:
        digitalWrite(S2, HIGH);
        digitalWrite(S3, HIGH);
        break;
  
      case BLUE:
        digitalWrite(S2, LOW);
        digitalWrite(S3, HIGH);
        break;
  
      default:
        digitalWrite(S2, HIGH);
        digitalWrite(S3, LOW);
        break;
  }
}


void mostrarSalida(int modoOut){
    switch (modoOut) {     
      case CALIBRAR:

        lcd.setCursor(0,0);
        lcd.print("FREQ: ");
        lcd.print("R:");
        lcd.print(freqRed);
        lcd.print(" ");
        // Cursor en la 11° posición de la primera fila
        lcd.setCursor(0,1);
        lcd.print("G=");
        lcd.print(freqGreen); //
        lcd.print(" ");
        // Cursor en la 11° posición de la 2° fila
        lcd.print("B=");
        lcd.print(freqBlue);
        lcd.print("  ");

        Serial.print(freqRed);
        Serial.print("\t");
        Serial.print(freqGreen);
        Serial.print("\t");
        Serial.println(freqBlue);
        Serial.print("\t");
      break;
  
      case NORMAL:

        lcd.setCursor(0,0);
        lcd.print("COLOR: ");
        lcd.print("R:");
        lcd.print(colorR);//1 decimal
        lcd.print(" ");
        // Cursor en la 11° posición de la primera fila
        lcd.setCursor(0,1);
        lcd.print("G=");
        lcd.print(colorG); //
        lcd.print(" ");
        // Cursor en la 11° posición de la 2° fila
        lcd.print("B=");
        lcd.print(colorB);
        lcd.print("  ");

        Serial.print(colorR);
        Serial.print(",");
        Serial.print(colorG);
        Serial.print(",");
        Serial.print(colorB);
        Serial.println("");
        break;

    }
}

void setup() {

  // Definición de las Salidas
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(LUZ, OUTPUT);

  // Definición de salida de sensor como entrada de datos
  pinMode(OUT, INPUT);

  // Definición de intensidad de luz
  analogWrite(LUZ, 255);

  // Definición de la escala de frecuencia
  modoFreq(FREQ_100);

  // Incializaciones del timer
  inicioTimer = millis(); // inicializa el timer - La función millis() me devuelve el número de milisegundos desde que Arduino se ha reseteado, dado que millis devuelve un unsigned long puede tener un valor de hasta 4.294.967.295 que son 49 días, 17 horas, 2 minutos, 47 segundos y 292 milisegundos, por lo tanto al cabo de ese periodo el contador se resetea.
  timerOn = true; // Activa el timer

  // Inicializacion de filtro
  modoFiltro(RED);

  // Display LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  Serial.begin(9600);
}



/*
*   Ejecuta un modelo de CUR, carga en la variable freqMedida el resultado y devuelve true cuando dicho resultado es válido
*/
bool cur(int milis){

  lectura = digitalRead(OUT);

  // Sincronismo con la ventana de tiempo
  if (timerOn) {
    // Conteo de flancos ascendentes
    if (lectura > lectAnterior) {
      acumLect += lectura; 
    }
    lectAnterior = lectura;

    // Chequea si el tiempo del timer caducó
    deltaT = (millis() - inicioTimer);
    if (deltaT >= milis) {
      inicioTimer = millis(); // se resetea el timer

      // Cantidad de ciclos en ventana de tiempo (16mil ciclos por miliseg)
      acumClock = (16*1000)*deltaT; 
      
      freqMedida = acumLect*(F_CPU/acumClock); // Ecuación de escala! (Fx = Nx*Fr/Nr)
      
      acumLect = 0;
      acumClock = 0;
      timerOn = false; // se desactiva el timer
      
      return true;
    }
  }
  return false;
}

void loop() {
  // Ejecuta CUR en cada ciclo y evalúa si ya devolvió un dato válido
  if(cur(250)){
    // Se alterna entre 3 turnos para medir R,G y B con el mismo CUR
    switch (turno)
    {
    case 0:
      freqRed = freqMedida;
      colorR = (freqRed - FREQ_R_MIN*4) * 255 / ((FREQ_R_MAX - FREQ_R_MIN)*4);
      if(colorR < 0){colorR = 0;} // saturación inferior
      if(colorR > 255){colorR = 255;} // saturación superior
      modoFiltro(GREEN); // se setea el filtro siguiente
      turno++;
      break;

    case 1:
      freqGreen = freqMedida;      
      colorG = (freqGreen - FREQ_G_MIN*4) * 255 / ((FREQ_G_MAX - FREQ_G_MIN)*4);
      if(colorG < 0){colorG = 0;} // saturación inferior
      if(colorG > 255){colorG = 255;} // saturación superior
      modoFiltro(BLUE); // se setea el filtro siguiente
      turno++;
      break;
      
    case 2:
      freqBlue = freqMedida;      
      colorB = (freqBlue - FREQ_B_MIN*4) * 255 / ((FREQ_B_MAX - FREQ_B_MIN)*4);
      if(colorB < 0){colorB = 0;} // saturación inferior
      if(colorB > 255){colorB = 255;} // saturación superior
      modoFiltro(RED); // se setea el filtro siguiente
      turno++;   
      break;

    default:      
      turno = 0;
      mostrarSalida(NORMAL);      
      break;
    }
    timerOn = true;
  }
}
