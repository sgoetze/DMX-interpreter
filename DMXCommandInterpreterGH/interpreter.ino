
//find a char that seperate commands - worker for findCmd()
int16_t seek_pos (String str, int16_t pos, int16_t dir) {  //find in str (multi, runni)
  char c = str[pos];
  int16_t e = str.length();
  if ((pos > 0) && ((c=='\n') || (c==';')))   //starting with line- or command-end
    c = str[--pos]; 
  while (pos>=0 && pos<e && c!='\n' && c!=';') {
    pos += dir;
    if (pos < 0) return (pos);
    c = str[pos];
  }
  return (pos);
}

//locate and separate one instruction in command buffer "multi" or "runni"
//input param: current position, direction (previous, current, next) as (-1, 0, 1)
//output to pointed vars: first position, length
//special: *first == -1 -> no more line in this direction (-1, 1) or no line at all (0) 

void findCmd(String str, int16_t cPos, int16_t where, int16_t *first, int16_t *behind) {
  int16_t endPos, be = str.length();
  *behind = 0;
  if ((be == 0) || (cPos > be))  {
    *first = -1;
    return;
  }
  if (cPos == be){
    if ( where < 1) {
      cPos--;
    } else {
      *first = -1;
      return;
    }
  }
  if (where < 0) {
    endPos = seek_pos(str, cPos, -1);        //seek command
    if (endPos <= 0) {                   //already str[0] reached
      *first = -1;
      return;  
    }
    cPos = seek_pos(str, endPos - 1, -1);
    cPos++;
  } else if (where > 0) {
    cPos = seek_pos(str, cPos, 1);
    cPos++;
    if (cPos >= be) {         // already end of str[] reached
      *first = -1;            // possibly last char is ';'
      return;
    }
    endPos = seek_pos(str, cPos, 1);
  } else {
    cPos = seek_pos(str, cPos, -1);
    cPos++;
    endPos = seek_pos(str, cPos, 1);       
  }
  *first = cPos;
  *behind = endPos;
}

//check the last character in cmd line for 'S'(second) or 'M' (minute)
//returns the multiplicator for 1/10s, means 10 or 600 
int32_t sizeUnit(String line, bool fade) {
  char c = line[line.length()-1];
  if (c == 'S') return (fade ? 4 : 10);
  if (c == 'M') return (fade ? 236 : 600);
  return 1;
}

//remove comments (parts of string between given char)
//returns empty String if comment chars not in pairs
String noComments(String inStr, char searchChr) {
  String outStr = "";
  int16_t pos1, pos2;
  while (true) {
    pos1 = inStr.indexOf(searchChr);
    pos2 = inStr.indexOf(searchChr, pos1 + 1);
    if (pos1 == -1) return outStr + inStr;
    if (pos2 == -1) return "";
    outStr.concat(inStr.substring(0, pos1));
    inStr = inStr.substring(pos2 + 1);
  }  
}

//remove unnessecary characters
String noUseless(String str) {
  str.toUpperCase();
  str.replace("\r", "");
  str.replace(" ","");
  str.replace("\t","");
  while (str.indexOf("\n\n") != -1 ) 
    str.replace("\n\n", "\n");
  while (str.indexOf(";;") != -1) 
    str.replace(";;", ";");
  while (str.indexOf(";\n") != -1) 
    str.replace(";\n", "\n");
  while (str.indexOf("\n;") != -1) 
    str.replace("\n;", "\n");
  return(str);
}

//interpreter of one command copied from the instruction sequence in "multi" or "runni"
//if all of the commadn can be done the function does it
//if at the end there is a time to wait -> a timer is set
//if many actions have to be done time controlled (fade), an execute function is activated
int16_t cmdInterpreter(String line, bool execNow) {
  int16_t *strobeP;
  int16_t startChn, endChn;
  int16_t val[MAX_VAL] = {0}; 
  uint8_t index = 0;
  uint8_t *bufP;
  char tmpC;
  int16_t tmpI, tmp2I, valI;
  int32_t tmpL;
  
  if (line.length()==0)       //nothing to do
    return 0;
  //analyze line
  switch (line[0]) {
    case 'B': {                //instruction 'B'
      tmpC = line.charAt(1);  //following '1' or '2'
      if (tmpC == '1')        //to select dmxBuffer
        bufP = dmxBuffer1;
      else if(tmpC == '2')
        bufP = dmxBuffer2;
      else 
        return -2;            // return < 0 -> error, abs value = position in line
      if (line.charAt(2) != ':')  //following ':'
        return -3;
      line = line.substring(3);  
      startChn = line.toInt();      //following one or two int (divided by ',')
      if ((startChn < 1) || (startChn > DMX_PACKET_SIZE))
        return -4;  
      tmpI = line.indexOf('-');     //is '-' in address area dividing two int?
      if (tmpI < 0)
        tmpI = line.indexOf(',');   //is ',' in address area dividing two int?        
      tmp2I = line.indexOf('=');
      if (tmp2I < 0)
        return (tmpI<0)?-5:-1*(tmpI+3);   //kein '='
      if (tmpI < tmp2I) {                 //yes - ',' found in front of '='
        line = line.substring(tmpI+1);    //looking behind ','
        endChn = line.toInt();
        if ((endChn < 1) || (endChn > DMX_PACKET_SIZE))
          return -1*(1+tmpI); 
      } else  //here if [',''-'] behind '=' or [',''-'] not in line at all
        endChn = startChn;           //no endChn written 
      if (endChn < startChn)  //if it has smaller value -> error
        return -1*(1+tmpI);
      tmpI = line.indexOf('=');      //find '=' again in shorted line
      line = line.substring(tmpI+1);
      tmpI = 0;                  
      while(index < MAX_VAL) {    // collecting values behind '=' (','-separated)
        valI = line.toInt();
        if ((valI == 0) && (line.charAt(0) != '0'))  //not the best solution
          return -1*(tmpI+tmp2I);
        val[index++] = constrain(valI,-1,255);       //value found
        tmpI = line.indexOf(','); //one or one more ',' ?
        if (tmpI < 0)             //no 
          break;  //here only breaking while(..)
        line = line.substring(++tmpI);  
      } 
      int16_t i = startChn;
      while ((i <= endChn) || (i == startChn)) { //do once or until endChn reached
        uint8_t j = 0;
        do {                          //once write all values or stop on endChn
          if (val[j]>=0)
            *(bufP + i++) = val[j++];
          else
            j++;
          if ((i > endChn) && (endChn != 0)) break;     //break do while loop  
        } while (j < index);
        if(i > endChn) break;        //else outer while loop once more
      }
      if (execNow) {          //button "Execute", not for runMode"
        memset(buffer_dmx,0,DMX_PACKET_SIZE);
        memcpy(buffer_dmx+startChn, bufP+startChn, endChn-startChn+1);
      }
      break;                  //case 'B' finished
    }

    case '>': {                //instruction '>' shifting from startChn to endChn
      tmpC = line.charAt(1);  //following '1' or '2'
      if (tmpC == '1')        //to select dmxBuffer
        bufP = dmxBuffer1;
      else if(tmpC == '2')
        bufP = dmxBuffer2;
      else 
        return -2;            // return < 0 -> error, abs value = position in line
      if (line.charAt(2) != ':')  //following ':'
        return -3;
      line = line.substring(3);  
      startChn = line.toInt();      //following one or two int (divided by ',')
      if ((startChn < 1) || (startChn > DMX_PACKET_SIZE))
        return -4;  
      tmpI = line.indexOf('-');     //is '-' in address area dividing two int?
      if (tmpI < 0) {
        tmpI = line.indexOf(',');   //is ',' in address area dividing two int?
        if (tmpI < 0)
          return -5;                //for shifting startChn and endChn must be given
      }          
      tmp2I = line.indexOf('=');
      if (tmp2I < 0)
        return (tmpI<0)?-5:-1*(tmpI+3);   //kein '='
      if (tmpI < tmp2I) {                 //yes - ',' found in front of '='
        line = line.substring(tmpI+1);    //looking behind ','
        endChn = line.toInt();
        if ((endChn < 1) || (endChn > DMX_PACKET_SIZE) || (endChn <= startChn))
          return -1*(1+tmpI); 
      } else
        return -1 * tmp2I;           //'=' in front of ',' or '-', no endChn parameter, ',' seperates values
      tmpI = line.indexOf('=');      //find '=' again in shorted line
      line = line.substring(tmpI+1);
      tmpI = 0;
      while(index < MAX_VAL) {    // collecting values behind '=' (','-separated)
        valI = line.toInt();
        if ((valI == 0) && (line.charAt(0) != '0'))  //not the best solution
          return -1*(tmpI+tmp2I);
        val[index++] = constrain(valI,-1,255);       //value found
        tmpI = line.indexOf(','); //one or one more ',' ?
        if (tmpI < 0)             //no 
          break;  //here only breaking while(..)
        line = line.substring(++tmpI);  
      }
      int16_t shift = endChn - index;
      if (shift < startChn -1)           //not enough channels between startChn and EndChn for values
        return -1*(tmpI+tmp2I);
      while (shift >= startChn) {
        *(bufP + shift + index) = *(bufP + shift);
        shift--;
      }
      for (int16_t j = 0; j < index; j++) {
        if (val[j] >= 0)
          *(bufP + startChn + j) = val[j];    //fill the channels that became free with given values 
      }
      if (execNow) {          //button "Execute", not for runMode"
        memset(buffer_dmx,0,DMX_PACKET_SIZE);
        memcpy(buffer_dmx+startChn, bufP+startChn, endChn-startChn+index);
      }
      break;                  //case '>' finished
    }

    case '<': {                //instruction '<' shifting from endChn to startChn
      tmpC = line.charAt(1);  //following '1' or '2'
      if (tmpC == '1')        //to select dmxBuffer
        bufP = dmxBuffer1;
      else if(tmpC == '2')
        bufP = dmxBuffer2;
      else 
        return -2;            // return < 0 -> error, abs value = position in line
      if (line.charAt(2) != ':')  //following ':'
        return -3;
      line = line.substring(3);  
      startChn = line.toInt();      //following one or two int (divided by ',')
      if ((startChn < 1) || (startChn > DMX_PACKET_SIZE))
        return -4;  
      tmpI = line.indexOf('-');     //is '-' in address area dividing two int?
      if (tmpI < 0) {
        tmpI = line.indexOf(',');   //is ',' in address area dividing two int?
        if (tmpI < 0)
          return -5;                //for shifting startChn and endChn must be given
      }          
      tmp2I = line.indexOf('=');
      if (tmp2I < 0)
        return (tmpI<0)?-5:-1*(tmpI+3);   //kein '='
      if (tmpI < tmp2I) {                 //yes - ',' found in front of '='
        line = line.substring(tmpI+1);    //looking behind ','
        endChn = line.toInt();
        if ((endChn < 1) || (endChn > DMX_PACKET_SIZE) || (endChn <= startChn))
          return -1*(1+tmpI); 
      } else
        return -1 * tmp2I;           //'=' in front of ',' or '-', no endChn parameter, ',' seperates values
      tmpI = line.indexOf('=');      //find '=' again in shorted line
      line = line.substring(tmpI+1);
      tmpI = 0;
      while(index < MAX_VAL) {    // collecting values behind '=' (','-separated)
        valI = line.toInt();
        if ((valI == 0) && (line.charAt(0) != '0'))  //not the best solution
          return -1*(tmpI+tmp2I);
        val[index++] = constrain(valI,-1,255);       //value found
        tmpI = line.indexOf(','); //one or one more ',' ?
        if (tmpI < 0)             //no 
          break;  //here only breaking while(..)
        line = line.substring(++tmpI);  
      }
      int16_t shift = startChn + index;
      if (shift > endChn + 1)           //not enough channels between startChn and EndChn for values
        return -1*(tmpI+tmp2I);
      while (shift <= endChn) {
        *(bufP + shift - index) = *(bufP + shift);
        shift++;
      }
      for (int16_t j = 0; j < index; j++) {
        if (val[j] >= 0)
          *(bufP + endChn - index + (j + 1)) = val[j];    //fill the channels that became free with given values 
      }
      if (execNow) {          //button "Execute", not for runMode"
        memset(buffer_dmx,0,DMX_PACKET_SIZE);
        memcpy(buffer_dmx+startChn, bufP+startChn, endChn-startChn+index);
      }
      break;                  //case '<' finished
    }
      
    case 'D': {               //instruction 'D'
      tmpC = line.charAt(1);  //following '1' or '2'
      if (tmpC == '1')        //to select dmxBuffer
        bufP = dmxBuffer1;
      else if(tmpC == '2')
        bufP = dmxBuffer2;
      else 
        return -2;            // return < 0 -> error, abs value = position in line
      if (line.charAt(2) != ':')  //following ':'
        return -3;
      line = line.substring(3);   //getting one int32_t
      tmpL = line.toInt();
      if (tmpL < 1)
        return -4;         
      memcpy(buffer_dmx, bufP, DMX_PACKET_SIZE);   //lights on
      lastSourceBuffer = tmpC - '0';
      if (!execNow) {
        displayWait = true;
        runTick = millis() + 100L * tmpL * sizeUnit(line, false);
        runState = 0xd0;
      }  
      break;                //case 'D' finished
    }
 
    case 'F': {            //instruction 'F'
      if (execNow) {
        logWrite(FADERUN);
        return -1;
      }
      tmpC = line.charAt(1);  //following '1' or '2'
      if (tmpC == '1') {        //to select dmxBuffer
        bufPfrom = dmxBuffer1;
        bufPto = dmxBuffer2;
      } else if(tmpC == '2') {
        bufPfrom = dmxBuffer2;
        bufPto = dmxBuffer1;
      } else 
        return -2;            // return < 0 -> error, abs value = position in line
      if (line.charAt(2) != ':')  //following ':'
        return -3;
      line = line.substring(3);   //getting one uint16_t
      tmpI = line.toInt();
      if (tmpI < 1)
        return -4; 
      for (uint16_t i = 1; i < DMX_PACKET_SIZE; i++) {    //calculate what to do
        fade_diff[i] = abs(*(bufPfrom + i) - *(bufPto + i));
      } //for
      fade_counter = 8;                // 254 values, min. fadetime 0,25s <-> tick = 1ms
      fade_timer = FADE_TIME * tmpI * sizeUnit(line, true);  //  tick has parameter and sizeUnit as multiplicators
      lastSourceBuffer = tmpC - '0';
      memcpy(buffer_dmx, bufPfrom, DMX_PACKET_SIZE);   //lights on for start condition
      fadeWait = true;
      runTick = millis() + fade_timer;
      runState = 0xf0;
      break;               //let the others do the work
    }
   
    case 'S': {             //instruction 'S'
      bool switchOff = false;      
      tmpC = line.charAt(1);  //following '0','1', '2' or ':'
      if ((tmpC != '0') && (tmpC != '1') && (tmpC != '2')) {
        line = line.substring(1);
        tmpC = '0';      
      } else
        line = line.substring(2);
      if (line.charAt(0) != ':') return -3; //no ':' at all
      tmp2I = line.indexOf('=');  //no seperator between channels and values -> only switch strobe off possible
      if (tmp2I < 0) {            //checking for strobe off
        if (line.charAt(1) != '0')  //ony "S:0", "S0:0", "S1:0", "S2:0" all the same = strobe off
          return -4;               //if not - error;
        //switch off strobe effect
        strobeOnVal = 0;
        strobeOffVal = 0;
        strobeEna = false;            
      } else {
        // here only S[0-2]:<startStr>[,-]<endStr>=<strobeVal> nearly the same as for B-cmd
        line = line.substring(1);
        startStr = line.toInt();      //following one or two int (divided by ',')
        if ((startStr < 1) || (startStr > DMX_PACKET_SIZE))
          return -4;  
        tmpI = line.indexOf('-');     //is '-' in address area dividing two int?
        if (tmpI < 0) {
          tmpI = line.indexOf(',');   //is ',' in address area dividing two int?
          if (tmpI < 0)
            endStr = startStr;                //no endStr given, only strobe for one channel
        } else {
          line = line.substring(tmpI+1);    //looking behind ','
          endStr = line.toInt();
          if ((endStr < 1) || (endChn > DMX_PACKET_SIZE) || (endStr < startStr))
            return -1*(4+tmpI);
        }           
        tmp2I = line.indexOf('=');
        if (tmp2I < 0)
          return -1 * (6+tmpI);   //kein '='
        tmpI = line.indexOf('=');      //find '=' again in shorted line
        line = line.substring(tmpI+1);
        tmpI = line.toInt();
        if ((tmpI < 0) || (tmpI > STROBE_TIME) || ((tmpI == 0) && (line.charAt(0) != '0')))
          return -1 * (1+tmp2I);
        if (tmpI == 0) {    //switch off strobe effect, the strangest way
          strobeOnVal = 0;
          strobeOffVal = 0;
          strobeEna = false;
        } else {
          strobeBuffer = (tmpC == ':') ? 0 : tmpC - '0'; //0 - always strobe, 1,2 - strobe only if buffer with number is source 
          strobeOnVal = STARTSTROBE;
          strobeOffVal = tmpI;
          strobeOn = 0;
          strobeOff = 0;
          strobeEna = true;
          
          if (execNow) {
            uint32_t tmpT = millis();
            if (endStrobeDemo > tmpT) {
              logWrite(EXECBLOCKED);
              return -1;
            }
          else {
              endStrobeDemo = millis() + 5000L;
              strobeBuffer = 0;
            }
          }
        }
      }
      break;               //case 'S' finished
    }
    
    case '{': {            //instruction '{' - loop start
      tmpI = 0;
      tmp2I = 0;
      if (line.charAt(1) == ':' ) {   //following ':'  -> there is a parameter
        tmpC = line.charAt(2);  
        if ((tmpC != 'T') && (tmpC != 'L'))  //following 'T' or 'L'
          return -3;
        if (line.charAt(3) != '=')  //following '='
          return -4;
        line = line.substring(4);
        valI = line.toInt();
        if (valI <= 0) 
          return -5;
        if (tmpC == 'T') tmpI = valI;
        if (tmpC == 'L') tmp2I = valI;        
      } else if (line.length() != 1) {
        return -2;  
      }
      if (loopIndex < MAX_LOOP) {
        loopMem[loopIndex].cmd = cmdCounter;
        loopMem[loopIndex].secs = tmpI ? (millis() + 100L * tmpI * sizeUnit(line, false)) : 0;
        loopMem[loopIndex].cnt = tmp2I;
        loopIndex++;
      } else {
        return -1;
      }    
      break;
    }               //case '{' finished

    case '}': {    //case '}'
      bool loopEnd = false;
      if  ((line.length() > 1) || (loopIndex == 0))
        return -1;
      loopIndex--;
      tmpI = loopMem[loopIndex].cnt;
      tmpL = loopMem[loopIndex].secs;
      if (tmpL > 0) {
        if (millis() >= tmpL) loopEnd = true;
      } else if (tmpI > 0) {
        tmpI--;
        if (tmpI > 0) loopMem[loopIndex].cnt = tmpI;
        else loopEnd = true;
      }
      if (!loopEnd) {
        cmdCounter = loopMem[loopIndex].cmd; 
        loopIndex++;
      }
      break;
    }          //case '}' finished
   
    default:
      return -1;
  }
  return 0;
}

// time controlled steps of D- and F-cmd
// called by loop() if displayWait or fadeWait set true
void doRun() {
  switch(runState) {
    case 0xd0: {  //only once per d-cmd -> time over, enable to run next cmd
        displayWait = false;
        runState = 0xff;    //0xd0 D -> waiting, 0xd1 -> D finished (is soon changed)
        break;
    }

    case 0xf0: {  //do fading between bufPfrom and bufPto
      for (uint16_t i = 1; i < DMX_PACKET_SIZE; i++) { 
        if (fade_diff[i]>=fade_counter) buffer_dmx[i] += (*(bufPto + i) > *(bufPfrom + i))?1:-1;
      }
      int16_t offset = (fade_counter > 247) ? 1 : 0;    
      fade_counter = (fade_counter + 8 + offset) % 256;   //8,16,..,248,1,9,..,249,2..,255  
      runTick = millis() + fade_timer;
      if (fade_counter == 255) {  //end of fading reached
        runState = 0xf1;
      }
      break;        
    }
    
    case 0xf1: {  //fade ready
      //jump over possibly killed steps because of strobe to final values
      memcpy(buffer_dmx, bufPto, DMX_PACKET_SIZE);
      fadeWait = false;
      runState = 0xff;
      break;  
    }

    //case 0xff: catched by default, should never be called

    default: { //catches programming errors
      logWrite(PROGERROR + String(runState,HEX));
      runMode = false;    //stop running
      break;
    }
  }
}
