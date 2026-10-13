
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Keypad.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define TFT_CS 22
#define TFT_DC 21
#define TFT_RST 4

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {13, 14, 16, 17};
byte colPins[COLS] = {25, 26, 27, 32};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const double PI_VAL = 3.14159265358979323846;
const double E_VAL = 2.71828182845904523536;

char expr[128] = "";
size_t exprLen = 0;

double lastResult = 0.0;
double memoryValue = 0.0;

bool hasResult = false;
bool hasMemory = false;
bool degreeMode = true;
bool functionMode = false;

class Parser {
  const char* p;
  bool valid;

  void spaces() {
    while (*p == ' ' || *p == '\t') p++;
  }

  bool eat(char c) {
    spaces();
    if (*p == c) {
      p++;
      return true;
    }
    return false;
  }

  double expression() {
    double v = term();

    while (valid) {
      if (eat('+')) v += term();
      else if (eat('-')) v -= term();
      else break;
    }

    return v;
  }

  double term() {
    double v = unary();

    while (valid) {
      if (eat('*')) v *= unary();
      else if (eat('/')) {
        double d = unary();

        if (d == 0.0) {
          valid = false;
          return 0;
        }

        v /= d;
      } else {
        break;
      }
    }

    return v;
  }

  double unary() {
    if (eat('+')) return unary();
    if (eat('-')) return -unary();
    return power();
  }

  double power() {
    double v = postfix();

    if (eat('^')) {
      v = pow(v, unary());
    }

    return v;
  }

  double postfix() {
    double v = primary();

    while (valid && eat('!')) {
      if (v < 0 || v > 170 || floor(v) != v) {
        valid = false;
        return 0;
      }

      double f = 1.0;

      for (int i = 2; i <= (int)v; i++) {
        f *= i;
      }

      v = f;
    }

    return v;
  }

  double primary() {
    spaces();

    if (eat('(')) {
      double v = expression();

      if (!eat(')')) valid = false;

      return v;
    }

    if (isdigit((unsigned char)*p) || *p == '.') {
      char* endPtr;
      double v = strtod(p, &endPtr);

      if (endPtr == p) {
        valid = false;
        return 0;
      }

      p = endPtr;
      return v;
    }

    if (isalpha((unsigned char)*p)) {
      char name[16];
      int n = 0;

      while (isalpha((unsigned char)*p)) {
        if (n < 15) {
          name[n++] = tolower((unsigned char)*p);
        }

        p++;
      }

      name[n] = '\0';

      if (strcmp(name, "pi") == 0) return PI_VAL;
      if (strcmp(name, "e") == 0) return E_VAL;

      if (!eat('(')) {
        valid = false;
        return 0;
      }

      double x = expression();

      if (!eat(')')) {
        valid = false;
        return 0;
      }

      double angle = degreeMode ? x * PI_VAL / 180.0 : x;
      double v = 0.0;

      if (strcmp(name, "sin") == 0) {
        v = sin(angle);
      } else if (strcmp(name, "cos") == 0) {
        v = cos(angle);
      } else if (strcmp(name, "tan") == 0) {
        v = tan(angle);
      } else if (strcmp(name, "asin") == 0) {
        if (x < -1 || x > 1) valid = false;
        else {
          v = asin(x);
          if (degreeMode) v *= 180.0 / PI_VAL;
        }
      } else if (strcmp(name, "acos") == 0) {
        if (x < -1 || x > 1) valid = false;
        else {
          v = acos(x);
          if (degreeMode) v *= 180.0 / PI_VAL;
        }
      } else if (strcmp(name, "atan") == 0) {
        v = atan(x);
        if (degreeMode) v *= 180.0 / PI_VAL;
      } else if (strcmp(name, "sqrt") == 0) {
        if (x < 0) valid = false;
        else v = sqrt(x);
      } else if (strcmp(name, "abs") == 0) {
        v = fabs(x);
      } else if (strcmp(name, "ln") == 0) {
        if (x <= 0) valid = false;
        else v = log(x);
      } else if (strcmp(name, "log") == 0) {
        if (x <= 0) valid = false;
        else v = log10(x);
      } else if (strcmp(name, "exp") == 0) {
        v = exp(x);
      } else {
        valid = false;
      }

      return v;
    }

    valid = false;
    return 0;
  }

public:
  Parser(const char* input) : p(input), valid(true) {}

  double evaluate() {
    double result = expression();
    spaces();

    if (*p != '\0' || !isfinite(result)) {
      valid = false;
    }

    return valid ? result : NAN;
  }
};

void appendText(const char* s) {
  size_t n = strlen(s);

  if (exprLen + n >= sizeof(expr)) return;

  memcpy(expr + exprLen, s, n);
  exprLen += n;
  expr[exprLen] = '\0';
}

void clearExpression() {
  exprLen = 0;
  expr[0] = '\0';
  hasResult = false;
}

void drawScreen(const char* error = nullptr) {
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(8, 8);
  tft.print("SCIENTIFIC CALC");

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(8, 35);
  tft.print(degreeMode ? "ANGLE: DEG" : "ANGLE: RAD");

  tft.setCursor(240, 35);
  tft.print(functionMode ? "FUNC" : "READY");

  tft.drawFastHLine(8, 52, 304, ILI9341_DARKGREY);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(8, 64);
  tft.print("Expression:");

  int start = exprLen > 24 ? exprLen - 24 : 0;

  tft.setCursor(8, 91);
  tft.print(expr + start);

  tft.drawFastHLine(8, 128, 304, ILI9341_DARKGREY);

  tft.setTextColor(ILI9341_GREEN);
  tft.setCursor(8, 143);
  tft.print("Result:");

  tft.setCursor(8, 173);

  if (error) {
    tft.setTextColor(ILI9341_RED);
    tft.print(error);
  } else if (hasResult) {
    tft.setTextColor(ILI9341_GREEN);
    tft.print(lastResult, 6);
  } else {
    tft.setTextColor(ILI9341_WHITE);
    tft.print("...");
  }

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setCursor(8, 215);
  tft.print("A=CLEAR B=BACK C=EQUAL");

  tft.setCursor(8, 230);
  tft.print("D=FUNC *=MUL #=DECIMAL");
}

void calculate() {
  if (exprLen == 0) {
    drawScreen("Empty expression");
    return;
  }

  Parser parser(expr);
  double result = parser.evaluate();

  if (!isfinite(result)) {
    hasResult = false;
    drawScreen("Math/Syntax error");
    return;
  }

  lastResult = result;
  hasResult = true;
  drawScreen();
}

void backspace() {
  if (exprLen > 0) {
    expr[--exprLen] = '\0';
  }

  hasResult = false;
  drawScreen();
}

void handleFunction(char key) {
  functionMode = false;

  switch (key) {
    case '1': appendText("sin("); break;
    case '2': appendText("cos("); break;
    case '3': appendText("tan("); break;
    case '4': appendText("sqrt("); break;
    case '5': appendText("ln("); break;
    case '6': appendText("log("); break;
    case '7': appendText("asin("); break;
    case '8': appendText("acos("); break;
    case '9': appendText("atan("); break;
    case '0': appendText("exp("); break;
    case '*': appendText("pi"); break;
    case '#': appendText("e"); break;
    case 'A': appendText("^"); break;
    case 'B': appendText("!"); break;

    case 'C':
      degreeMode = !degreeMode;
      break;

    case 'D':
      functionMode = true;
      break;
  }

  hasResult = false;
  drawScreen();
}

void handleKey(char key) {
  if (functionMode) {
    handleFunction(key);
    return;
  }

  switch (key) {
    case 'A':
      clearExpression();
      drawScreen();
      break;

    case 'B':
      backspace();
      break;

    case 'C':
      calculate();
      break;

    case 'D':
      functionMode = true;
      drawScreen();
      break;

    case '*':
      appendText("*");
      hasResult = false;
      drawScreen();
      break;

    case '#':
      appendText(".");
      hasResult = false;
      drawScreen();
      break;

    default: {
      char s[2] = {key, '\0'};
      appendText(s);
      hasResult = false;
      drawScreen();
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);

  SPI.begin(18, 19, 23, TFT_CS);

  tft.begin();
  tft.setRotation(1);

  drawScreen();

  Serial.println("ESP32 Scientific Calculator Ready");
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    handleKey(key);
  }
}
