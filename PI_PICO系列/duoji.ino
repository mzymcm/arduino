///===PWM信号与360舵机转速的关系

int servopin = 15; // 控制引脚编号

// 可调节参数
int rotationSpeed = 50;    // 速度百分比 (0-100)
int rotateTime = 5;        // 旋转时间 (毫秒)
int stopTime = 2000;       // 停止时间 (毫秒)
bool continuousMode = true; // 是否连续循环
bool reverseMode = false;   // 反转模式标志

void setup() {
  pinMode(servopin, OUTPUT);
  Serial.begin(9600);
  Serial.println("可调节参数的循环控制");
  Serial.println("增加反转控制功能");
  printParameters();
}

void loop() {
  static unsigned long lastActionTime = 0;
  static bool isRotating = false;
  
  // 检查串口输入
  if (Serial.available() > 0) {
    adjustParameters();
  }
  
  // 如果是连续模式，执行循环
  if (continuousMode) {
    unsigned long currentTime = millis();
    
    if (isRotating) {
      // 正在旋转
      if (currentTime - lastActionTime >= rotateTime) {
        // 旋转时间到，停止
        isRotating = false;
        lastActionTime = currentTime;
        sendStopSignal();
        Serial.println("停止");
      } else {
        // 继续旋转
        sendRotateSignal(rotationSpeed, reverseMode);
      }
    } else {
      // 正在停止
      if (currentTime - lastActionTime >= stopTime) {
        // 停止时间到，开始旋转
        isRotating = true;
        lastActionTime = currentTime;
        String direction = reverseMode ? "反向" : "正向";
        Serial.println("旋转 (方向: " + direction + ", 速度: " + String(rotationSpeed) + "%)");
      } else {
        // 继续停止
        sendStopSignal();
      }
    }
  }
}

// 发送旋转信号（支持正反转）
void sendRotateSignal(int speedPercent, bool reverse) {
  // 根据PWM信号与360舵机转速的关系计算脉冲宽度
  // 0.5ms (500us) - 正向最大转速
  // 1.5ms (1500us) - 速度为0
  // 2.5ms (2500us) - 反向最大转速
  
  // 将速度百分比(-100到100)转换为脉冲宽度(500到2500微秒)
  int pulseWidth;
  
  if (reverse) {
    // 反向旋转：速度为正时，脉冲宽度从1500到2500
    pulseWidth = 1500 + (speedPercent * 10); // 每1%速度对应10us
  } else {
    // 正向旋转：速度为正时，脉冲宽度从1500到500
    pulseWidth = 1500 - (speedPercent * 10); // 每1%速度对应10us
  }
  
  // 限制脉冲宽度在有效范围内
  pulseWidth = constrain(pulseWidth, 500, 2500);
  
  // 发送PWM信号
  digitalWrite(servopin, HIGH);
  delayMicroseconds(pulseWidth);
  digitalWrite(servopin, LOW);
  
  // 保持20ms周期（50Hz）
  delayMicroseconds(20000 - pulseWidth);
}

// 发送停止信号
void sendStopSignal() {
  digitalWrite(servopin, HIGH);
  delayMicroseconds(1500);  // 1.5ms脉冲宽度对应停止状态
  digitalWrite(servopin, LOW);
  delayMicroseconds(18500); // 保持20ms周期
}

// 调整参数
void adjustParameters() {
  char cmd = Serial.read();
  
  switch (cmd) {
    case '+':  // 增加速度
      rotationSpeed = min(100, rotationSpeed + 5);
      Serial.print("速度: ");
      Serial.println(rotationSpeed);
      break;
      
    case '-':  // 减少速度
      rotationSpeed = max(0, rotationSpeed - 5);
      Serial.print("速度: ");
      Serial.println(rotationSpeed);
      break;
      
    case 't':  // 增加旋转时间
      rotateTime += 100;
      Serial.print("旋转时间: ");
      Serial.println(rotateTime);
      break;
      
    case 'T':  // 减少旋转时间
      rotateTime = max(100, rotateTime - 100);
      Serial.print("旋转时间: ");
      Serial.println(rotateTime);
      break;
      
    case 's':  // 增加停止时间
      stopTime += 100;
      Serial.print("停止时间: ");
      Serial.println(stopTime);
      break;
      
    case 'S':  // 减少停止时间
      stopTime = max(100, stopTime - 100);
      Serial.print("停止时间: ");
      Serial.println(stopTime);
      break;
      
    case 'c':  // 切换连续模式
      continuousMode = !continuousMode;
      Serial.print("连续模式: ");
      Serial.println(continuousMode ? "开启" : "关闭");
      break;
      
    case 'r':  // 切换反转模式
      reverseMode = !reverseMode;
      Serial.print("反转模式: ");
      Serial.println(reverseMode ? "开启" : "关闭");
      break;
      
    case 'f':  // 正向旋转
      reverseMode = false;
      Serial.println("方向: 正向");
      break;
      
    case 'b':  // 反向旋转
      reverseMode = true;
      Serial.println("方向: 反向");
      break;
      
    case ' ':  // 单次执行
      singleCycle();
      break;
      
    case 'p':  // 打印参数
      printParameters();
      break;
      
    case 'h':  // 显示帮助
      showHelp();
      break;
  }
}

// 执行单次循环
void singleCycle() {
  Serial.println("执行单次循环...");
  String direction = reverseMode ? "反向" : "正向";
  Serial.println("方向: " + direction);
  
  // 旋转
  Serial.println("开始旋转");
  unsigned long startTime = millis();
  while (millis() - startTime < rotateTime) {
    sendRotateSignal(rotationSpeed, reverseMode);
  }
  
  // 停止
  Serial.println("停止");
  startTime = millis();
  while (millis() - startTime < stopTime) {
    sendStopSignal();
  }
  
  Serial.println("单次循环完成");
}

// 打印当前参数
void printParameters() {
  Serial.println("========================");
  Serial.println("当前参数:");
  Serial.print("  旋转速度: ");
  Serial.print(rotationSpeed);
  Serial.println("%");
  Serial.print("  旋转时间: ");
  Serial.print(rotateTime);
  Serial.println("毫秒");
  Serial.print("  停止时间: ");
  Serial.print(stopTime);
  Serial.println("毫秒");
  Serial.print("  连续模式: ");
  Serial.println(continuousMode ? "开启" : "关闭");
  Serial.print("  旋转方向: ");
  Serial.println(reverseMode ? "反向" : "正向");
  Serial.println("========================");
}

// 显示帮助信息
void showHelp() {
  Serial.println("========================");
  Serial.println("控制命令:");
  Serial.println("  +     : 增加速度 (+5%)");
  Serial.println("  -     : 减少速度 (-5%)");
  Serial.println("  t/T   : 增加/减少旋转时间");
  Serial.println("  s/S   : 增加/减少停止时间");
  Serial.println("  c     : 切换连续模式");
  Serial.println("  r     : 切换反转模式");
  Serial.println("  f     : 设置为正向旋转");
  Serial.println("  b     : 设置为反向旋转");
  Serial.println("  空格  : 执行单次循环");
  Serial.println("  p     : 打印当前参数");
  Serial.println("  h     : 显示帮助信息");
  Serial.println("========================");
}