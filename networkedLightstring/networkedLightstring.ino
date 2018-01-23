
/** 
 * Listen on the network and respond to commands.  The server listens on  
 * port 23 (telnet port) for commands in this format
 * 
 *  <letter> <arguments>\n
 *  
 * For example
 * 
 *  w 10 FF FF FF\n
 *  
 * This command says to write the colour value for pixel 16 (all numbers are hex).
 * The command characeters supported are as follows
 * 
 *  w <pixel number> <red> <green> <blue> - write the given pixel with the RGB values
 *  r <pixel number> - read the current RGB values
 *  s - show the current pixel data, send it to the light string
 *  
 * All commands are cases sensitive and are terminated by a '\n' character.
 * 
 * TODO: Can multiple lines be sent in one go?
 * TODO: How will these arrive at the Arduino?
 */

#include <Adafruit_NeoPixel.h>

#include <EthernetClient.h>
#include <Ethernet.h>
#include <EthernetServer.h>

// This is the size of the buffer ...
const int COMMAND_BUFFER_SIZE = 13;

// ... can only fit this many chars as need to nul-terminate
const int MAXIMUM_COMMAND_LENGTH = COMMAND_BUFFER_SIZE - 1;

// String buffer that can hold hex version of command buffer
// Hex strings are 0xFF for example
const int STRING_BUFFER_SIZE = COMMAND_BUFFER_SIZE * 4;

// Used for the complete message
const int MESSAGE_BUFFER_SIZE = STRING_BUFFER_SIZE + 100;

// This is in the correct order to read the MAC 
// address.  It's going to be stored in 
// big-endian order anyway since this is an 8-bit
// micro-controller
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED  
};

// TODO: Need to use DHCP in the live version
IPAddress my_ip_address(192,168,1,177);
IPAddress my_dns_server_address(192,168,1,1);
IPAddress my_gateway_address(192,168,1,1);
IPAddress my_subnet(255,255,255,0);

// Listen on the telnet port
EthernetServer server(23);

/**
 * Convert the command buffer into a string.  This handles
 * non-printable (non-ASCII) characters.
 * 
 * NOTE: Make sure that the stringBuffer is at least 
 * four times the size of the COMMAND_BUFFER_SIZE so that it
 * can accomodate non-printing characters that are convcerted to 
 * hex, e.g. 0xff
 */
void convertCommandToString(byte* commandBuffer, char* stringBuffer)
{
  int positionInCommandBuffer=0;
  int positionInStringBuffer=0;

  while(positionInCommandBuffer < MAXIMUM_COMMAND_LENGTH) 
  {
    byte byteFromCommand = commandBuffer[positionInCommandBuffer];

    // In the case that this is a valid command it will be newline-
    // terminated and we don't want to print that out
    if( byteFromCommand == '\n' ) break;

    // The buffer should be 0 terminated and we don't want to print that
    if( byteFromCommand == '\0' ) break;
    
    if( byteFromCommand > 32 && byteFromCommand < 127)
    {
      stringBuffer[positionInStringBuffer] = (char)byteFromCommand;
      positionInStringBuffer++;
    }
    else
    {
      positionInStringBuffer += sprintf(
        &stringBuffer[positionInStringBuffer],"0x%x", byteFromCommand);
    }

    // Move onto next character
    positionInCommandBuffer++;
  }

  // Nul-terminate the string
  stringBuffer[positionInStringBuffer]='\0';
}


/** 
 * Send a message back to the client that is compose of a 
 * string followed by the contents of the command buffer
 */
void send_message_back_with_command(EthernetClient client, String message, byte* commandBuffer)
{
  Serial.print("Send message back with command: ");
  Serial.println(message);

  char messageBuffer[MESSAGE_BUFFER_SIZE] = {};
  char commandStringBuffer[STRING_BUFFER_SIZE] = {};

  convertCommandToString(commandBuffer, commandStringBuffer);
  
  snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "%s '%s'\n", message.c_str(), commandStringBuffer); 

  client.print(messageBuffer);
  
  // Need to return error messages as soon as possible
  client.flush();
}


/**
 * Execute the command
 */
void executeCommand(EthernetClient client, byte* commandBuffer)
{
  send_message_back_with_command(client, "Not implemented command yet", commandBuffer);
}


/**
 * For a client that is available, read all the bytes of 
 * data, copy these into a buffer for a single command.
 * The command shoukd be terminated by a '\n' character 
 * and once this is read it'll be processed and a 
 * response generated.
 * 
 * If no '\n' character is found within the size of the 
 * command buffer then we assume that this is garbage
 * and return an error saying so.  Reading of the data continues
 * until it has all been read.
 * 
 * TODO: What happens if one client keeps sending endless garbage
 * will that tie up the main loop() indefinitely such that 
 * nothing else can get done?  
 */
void readCommands(EthernetClient client)
{
  // There needs to be a nul-terminated string and 
  // this could be the very last character 
  byte commandBuffer[COMMAND_BUFFER_SIZE] = {};
  int currentCommandBufferIndex = 0;  

  Serial.println("Reading commands from client");
  
  while(client.available())
  {
    // This should always read a byte
    byte byteReadFromSocket = client.read();

    Serial.println("Read a byte");
    
    commandBuffer[currentCommandBufferIndex] = byteReadFromSocket;
    currentCommandBufferIndex++;

    // Make sure the current contents are nul-terminated - saves
    // writing this same code in several places below
    commandBuffer[currentCommandBufferIndex] = '\0';

    // We got a command to execute
    if (byteReadFromSocket == '\n')
    {
      Serial.println("Got new line");
      executeCommand(client, commandBuffer);
      
      currentCommandBufferIndex = 0;
      continue;
    }

    // Client just sent garbage that wasn't newline terminated
    if (currentCommandBufferIndex == MAXIMUM_COMMAND_LENGTH) 
    {
      Serial.println("Got garbage command");
      send_message_back_with_command(client, "ERROR: Garbled command: ", commandBuffer);
      
      currentCommandBufferIndex = 0;
      continue;
    }

    Serial.print("Current command buffer index: ");
    Serial.println(currentCommandBufferIndex);
  }

  // Might be some read bytes that are garbage 
  if (currentCommandBufferIndex > 0) 
  {
    Serial.println("Got garbage at end of line");
    send_message_back_with_command(client, "ERROR: Incomplete command: ", commandBuffer);
  }
}


/**
 * Initial setup for networking etc
 */
void setup() {
  Serial.begin(9600);
  Serial.println("Started serial console");
  
  Ethernet.begin(
    mac,
    my_ip_address, 
    my_dns_server_address,
    my_gateway_address,
    my_subnet);
    
  server.begin();
}


/**
 * Main Arduino loop
 */
void loop() {

  EthernetClient client = server.available();
  if (client) {
    Serial.println("Got a client");
    
    // For a connected client read any available bytes and 
    // interpret the commands - should return a response or error
    // message for all newline-terminated commands
    // TODO: What happens if there is no newline? 
    readCommands(client);    
  }
}
