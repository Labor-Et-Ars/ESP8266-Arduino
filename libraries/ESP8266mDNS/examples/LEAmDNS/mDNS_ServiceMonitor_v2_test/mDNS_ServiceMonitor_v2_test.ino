/*
    ESP8266 mDNS Responder Service Monitor

    This example demonstrates two features of the LEA MDNSResponder:
    1. The host and service domain negotiation process that ensures
     the uniqueness of the finally choosen host and service domain name.
    2. The dynamic MDNS service lookup/query feature.

    A list of 'HTTP' services in the local network is created and kept up to date.
    In addition to this, a (very simple) HTTP server is set up on port 80
    and announced as a service.

    The ESP itself is initially announced to clients as 'esp8266.local', if this host domain
    is already used in the local network, another host domain is negociated. Keep an
    eye to the serial output to learn the final host domain for the HTTP service.
    The service itself is is announced as 'host domain'._sapuducul._tcp.local.
    The HTTP server delivers a short greeting and the current  list of other 'HTTP' services (not updated).
    The web server code is taken nearly 1:1 from the 'mDNS_Web_Server.ino' example.
    Point your browser to 'host domain'.local to see this web page.

    Instructions:
    - Update WiFi SSID and password as necessary.
    - Flash the sketch to the ESP8266 board
    - Install host software:
    - For Linux, install Avahi (http://avahi.org/).
    - For Windows, install Bonjour (http://www.apple.com/support/bonjour/).
    - For Mac OSX and iOS support is built in through Bonjour already.
    - Use a browser like 'Safari' to see the page at http://'host domain'.local.

*/


#define END 0 // enable netdump

#ifndef STASSID
#define STASSID "ssid"
#define STAPSK "psk"
#endif


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#if END
#include <Netdump.h>
using namespace NetCapture;
Netdump nd;
#endif



/*
    Include the clsLEAMDNSHost (the library needs to be included also)
    As LEA clsLEAMDNSHost is experimantal in the ESP8266 environment currently, the
    legacy clsLEAMDNSHost is defaulted in th include file.
    There are two ways to access LEA clsLEAMDNSHost:
    1. Prepend every declaration and call to global declarations or functions with the namespace, like:
      'LEAmDNS:clsLEAMDNSHost::hMDNSService  hMDNSService;'
      This way is used in the example. But be careful, if the namespace declaration is missing
      somewhere, the call might go to the legacy implementation...
    2. Open 'ESP8266mDNS.h' and set LEAmDNS to default.

*/

#define MDNS2_EXPERIMENTAL
#include <ESP8266mDNS.h>
//#include "LocalDefines.h"
//#include "Netdump.h"
//using namespace NetCapture;
/*
    Global defines and vars
*/

clsLEAMDNSHost MDNSv2;

#define SERVICE_PORT                                    80                                  // HTTP port

char*                                          pcHostDomain            = 0;        // Negociated host domain
bool                                           bHostDomainConfirmed    = false;    // Flags the confirmation of the host domain
clsLEAMDNSHost::clsService*                     hMDNSService            = 0;        // The handle of the sapuducu service in the MDNS responder
clsLEAMDNSHost::clsQuery*                hMDNSServiceQuery       = 0;        // The handle of the 'espclk.tcp' service query in the MDNS responder

const String                                   cstrNoHTTPServices      = "Currently no 'http.tcp' services in the local network!<br/>";
String                                         strHTTPServices         = cstrNoHTTPServices;

// HTTP server at port 'SERVICE_PORT' will respond to HTTP requests
ESP8266WebServer                                     server(SERVICE_PORT);


/*
    setStationHostname
*/
bool setStationHostname(const char* p_pcHostname)
{

    if (p_pcHostname)
    {
        WiFi.hostname(p_pcHostname);
        Serial.printf("setStationHostname: Station hostname is set to '%s'\n", p_pcHostname);
        return true;
    }
    return false;
}

void MDNSServiceQueryCallback(const clsLEAMDNSHost::clsQuery& p_Query,
                              const clsLEAMDNSHost::clsQuery::clsAnswer& p_Answer,
                              clsLEAMDNSHost::clsQuery::clsAnswer::typeQueryAnswerType p_QueryAnswerTypeFlags,
                              bool p_bSetContent)
{
    Serial.printf("CB MDNSServiceQueryCallback\n");

    String answerInfo;
    clsLEAMDNSHost::clsQuery::clsAnswer::typeQueryAnswerType mask = 0;
    if (p_QueryAnswerTypeFlags & (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::ServiceDomain)
    {
        answerInfo += "(ServiceDomain ";
        answerInfo += p_Answer.m_ServiceDomain.c_str();
        answerInfo += ")";
        mask |= (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::ServiceDomain;
    }
    if (p_QueryAnswerTypeFlags & (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::HostDomainPort)
    {
        answerInfo += "(HostDomainAndPort ";
        answerInfo += p_Answer.m_HostDomain.c_str();
        answerInfo += ")";
        mask |= (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::HostDomainPort;
    }
    if (p_QueryAnswerTypeFlags & (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::IPv4Address)
    {
        answerInfo += "(IP4Address ";
        for (auto ip : p_Answer.m_IPv4Addresses)
        {
            answerInfo += "- ";
            answerInfo += ip->m_IPAddress.toString();
        }
        answerInfo += ")";
        mask |= (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::IPv4Address;
    }
#ifdef MDNS_IPV6_SUPPORT
    if (p_QueryAnswerTypeFlags & (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::IPv6Address)
    {
        answerInfo += "(IP6Address ";
        for (auto ip : p_Answer.m_IPv6Addresses)
        {
            answerInfo += "- ";
            answerInfo += ip->m_IPAddress.toString();
        }
        answerInfo += ")";
        mask |= (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::IPv6Address;
    }
#endif // MDNS_IPV6_SUPPORT
    if (p_QueryAnswerTypeFlags & (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::Txts)
    {
        answerInfo += "(TXT ";
        for (auto kv : p_Answer.m_Txts.m_Txts)
        {
            answerInfo += "kv: ";
            answerInfo += kv->m_pcKey;
            answerInfo += ": ";
            answerInfo += kv->m_pcValue;
        }
        answerInfo += ")";
        mask |= (uint8_t)clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::Txts;
    }
    if (p_QueryAnswerTypeFlags & ~mask)
    {
        answerInfo += "(other flags are set)";
    }

#if 0
    switch (p_QueryAnswerTypeFlags)
    {
    case static_cast<clsLEAMDNSHost::clsQuery::clsAnswer::typeQueryAnswerType>(clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::ServiceDomain):
        answerInfo = "ServiceDomain " + String(p_Answer.m_ServiceDomain.c_str());
        break;

    case static_cast<clsLEAMDNSHost::clsQuery::clsAnswer::typeQueryAnswerType>(clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::HostDomainPort):
        answerInfo = "HostDomainAndPort " + String(p_Answer.m_HostDomain.c_str()) + ":" + String(p_Answer.m_u16Port);
        break;
    case static_cast<clsLEAMDNSHost::clsQuery::clsAnswer::typeQueryAnswerType>(clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::IPv4Address):
        answerInfo = "IP4Address ";
        for (auto ip : p_Answer.m_IPv4Addresses)
        {
            answerInfo += "- " + ip->m_IPAddress.toString();
        };
        break;
    case static_cast<clsLEAMDNSHost::clsQuery::clsAnswer::typeQueryAnswerType>(clsLEAMDNSHost::clsQuery::clsAnswer::enuQueryAnswerType::Txts):
        answerInfo = "TXT ";
        for (auto kv : p_Answer.m_Txts.m_Txts)
        {
            answerInfo += "\nkv : " + String(kv->m_pcKey) + " : " + String(kv->m_pcValue);
        }
        break;
    default :
        answerInfo = "Unknown Answertype " + String(p_QueryAnswerTypeFlags);

    }
#endif

    Serial.printf("Answer %s %s\n", answerInfo.c_str(), p_bSetContent ? "Modified" : "Deleted");
}

/*
    MDNSServiceProbeResultCallback
    Probe result callback for Services
*/

void serviceProbeResult(clsLEAMDNSHost::clsService& p_rMDNSService,
                        const char* p_pcInstanceName,
                        bool p_bProbeResult)
{
    Serial.printf("MDNSServiceProbeResultCallback: Service %s probe %s\n", p_pcInstanceName, (p_bProbeResult ? "succeeded." : "failed!"));
}

/*
    MDNSHostProbeResultCallback

    Probe result callback for the host domain.
    If the domain is free, the host domain is set and the http service is
    added.
    If the domain is already used, a new name is created and the probing is
    restarted via p_pclsLEAMDNSHost->setHostname().

*/

void hostProbeResult(clsLEAMDNSHost & p_rMDNSHost, String p_pcDomainName, bool p_bProbeResult)
{

    Serial.printf("MDNSHostProbeResultCallback: Host domain '%s.local' is %s\n", p_pcDomainName.c_str(), (p_bProbeResult ? "free" : "already USED!"));

    if (true == p_bProbeResult)
    {
        // Set station hostname
        setStationHostname(pcHostDomain);
        Serial.printf("setting hostname = '%s'\n", pcHostDomain?: "nullptr");

        if (!bHostDomainConfirmed)
        {
            // Hostname free -> setup clock service
            bHostDomainConfirmed = true;

            if (!hMDNSService)
            {
                Serial.printf("adding service tcp.sapuducu port %d\n", SERVICE_PORT);
                // Add a 'sapuducu.tcp' service to port 'SERVICE_PORT', using the host domain as instance domain
                hMDNSService = MDNSv2.addService(0, "sapuducu", "tcp", SERVICE_PORT, serviceProbeResult);

                if (hMDNSService)
                {
                    Serial.printf("hMDNSService\n");
                    hMDNSService->setProbeResultCallback(serviceProbeResult);
                    //         MDNSv2.setServiceProbeResultCallback(hMDNSService, serviceProbeResult);

                    // Add some '_sapuducu._tcp' protocol specific MDNS service TXT items
                    // See: http://www.dns-sd.org/txtrecords.html#http
                    hMDNSService->addServiceTxt("user", "x");
                    hMDNSService->addServiceTxt("password", "y");
                    hMDNSService->addServiceTxt("path", "/");
                }
                else
                    Serial.printf("hMDNSService=0 ??\n");

                // Install dynamic 'espclk.tcp' service query
                if (!hMDNSServiceQuery)
                {
                    Serial.printf("installing hMDNSServiceQuery\n");
                    hMDNSServiceQuery = MDNSv2.installServiceQuery("espclk", "tcp", MDNSServiceQueryCallback);
                    if (hMDNSServiceQuery)
                    {
                        Serial.printf("MDNSProbeResultCallback: Service query for 'espclk.tcp' services installed.\n");
                    }
                    else
                    {
                        Serial.printf("MDNSProbeResultCallback: FAILED to install service query for 'espclk.tcp' services!\n");
                    }
                }
            }
        }
    }
    else
    {
        // Change hostname, use '-' as divider between base name and index
        if (clsLEAMDNSHost::indexDomainName(pcHostDomain, "-", 0))
        {
            MDNSv2.setHostName(pcHostDomain);
        }
        else
        {
            Serial.println("MDNSProbeResultCallback: FAILED to update hostname!");
        }
    }
}

/*
    HTTP request function (not found is handled by server)
*/
void handleHTTPRequest()
{
  Serial.println("");
  Serial.println("HTTP Request");

  IPAddress ip = server.client().localIP();
  String ipStr = ip.toString();
  String s = "<!DOCTYPE HTML>\r\n<html><h3><head>Hello from ";
  s += WiFi.hostname() + ".local at " + server.client().localIP().toString() + "</h3></head>";
  s += "<br/><h4>Local HTTP services are :</h4>";
  s += "<ol>";
 // hMDNSServiceQuery->


  if (hMDNSServiceQuery) {
    for (auto info : hMDNSServiceQuery->answerAccessors()) {
      s += "<li>";
      s += info.serviceDomain();

      if (info.hostDomainAvailable()) {
        s += "<br/>Hostname: ";
        s += String(info.hostDomain());
        s += (info.hostPortAvailable()) ? (":" + String(info.hostPort())) : "";
      }
      if (info.IPv4AddressAvailable()) {
        s += "<br/>IPv4:";
        for (auto ip : info.IPv4Addresses()) {
          s += " " + ip.toString();
        }
      }
#ifdef MDNS_IPV6_SUPPORT
      if (info.IPv6AddressAvailable()) {
        s += "<br/>IPv6:";
        for (auto ip : info.IPv6Addresses()) {
          s += " " + ip.toString();
        }
      }
#endif
      if (info.txtsAvailable()) {
        s += "<br/>TXT:<br/>";
        for (auto kv : info.txtKeyValues()) {
          s += "\t" + String(kv.first) + " : " + String(kv.second) + "<br/>";
        }
      }
      s += "</li>";
    }
  }
  s += "</ol><br/>";

  Serial.println("Sending 200");
  server.send(200, "text/html", s);
  Serial.println("Done with request");
}



/*
    setup
*/
void setup(void)
{
    Serial.begin(115200);
    //  nd.printDump(Serial, Packet::PacketDetail::NONE, [](const Packet& p){return(p.getInOut() &&  p.isMDNS());});
    Serial.setDebugOutput(false);

    // Connect to WiFi network
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Soft1");
    WiFi.begin(STASSID, STAPSK);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(STASSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Setup HTTP server
    server.on("/", handleHTTPRequest);

    // Setup MDNS responders
    MDNSv2.setProbeResultCallback(hostProbeResult);

    // Init the (currently empty) host domain string with 'esp8266'
    MDNSv2.begin("esp8266_v2");
    /*
        if ((!clsLEAMDNSHost::indexDomain(pcHostDomain, 0, "esp8266")) ||
          (!MDNSv2.begin(pcHostDomain))) {
        Serial.println(" Error setting up MDNS responder!");
        while (1) { // STOP
          delay(1000);
        }
        }
    */
    Serial.println("MDNS responder started");

    // Start HTTP server
    server.begin();
    Serial.println("HTTP server started");

#if END
    nd.printDump(Serial, Packet::PacketDetail::FULL);
#endif

}



void loop(void)
{
    // Check if a request has come in
    server.handleClient();
    // Allow MDNS processing
    MDNSv2.update();

    static esp8266::polledTimeout::periodicMs timeout(10000);
    if (timeout.expired())
    {
        Serial.printf("up=%lumn heap=%u\n", millis() / 1000 / 60, ESP.getFreeHeap());
    }
}



