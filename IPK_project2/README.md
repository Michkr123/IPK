# IPK project 2 - client for a chat server 
## Úvod

Tento projekt implementuje klienta pro chatovací server, který využívá protokol IPK25CHAT. Cílem projektu je vytvořit spolehlivého klienta, který umožňuje uživateli komunikovat se serverem pomocí textových zpráv, připojovat se do různých kanálů a přijímat odpovědi od serveru a adekvátně na ně reagovat. 

Klient podporuje dvě varianty komunikace – pomocí protokolu TCP a UDP.

Projekt je navržen tak, aby:
- <b>Zabezpečil spolehlivou komunikaci</b>: U TCP zajištěna spojením, u UDP klient po odeslání zprávy čeká na potvrzení nebo odpověď od serveru, přičemž využívá mechanismy pro opakované odesílání v případě, že potvrzovací zpráva není obdržena v určeném časovém intervalu.

- <b>Poskytoval jasné a uživatelsky přívětivé rozhraní</b>: Uživateli umožnil komunikovat se serverem pomocí předem pevně stanovených příkazů. Klient reaguje odesíláním požadovaných zpráv.

- <b>Zpracování uživatelského vstupu</b>:  Klient zpracovává jeden po druhém příkazy ze standartího vstupu, kontroluje správnost jejich užití a parametrů a následně připraví data vzhledem k použitému protokolu a odesílá je serveru.

- <b>Zpracování serverových zpráv</b>: Přijímá zprávy ze serveru vzhledem k použitému protokolu, zpracovává je a vypisuje do standardního výstupu.


Použitý IPK25CHAT protokol byl definován s ohledem na efektivní přenos dat, spolehlivost komunikace i robustnost při selhání – např. klient i server spolu komunikují pomocí předem daných zpráv s pevně stanovenou strukturou, kde jsou důležitými komponentami typ zprávy a identifikátor zprávy. Tyto prvky jsou při odesílání převedeny do síťového pořadí, což zajišťuje, že data budou interpretována správně bez ohledu na platformu.

Celkově tento projekt představuje komplexní řešení pro implementaci chatovacího klienta, jehož hlavním přínosem je ukázka správného řízení síťové komunikace, obsluhy chyb a synchronního zpracování uživatelských příkazů při komunikaci se serverem. Projekt tak slouží nejen jako praktická aplikace, ale i jako učební pomůcka pro pochopení zásad sítové komunikace pomocí protokolů TCP a UDP.

### Překlad a spuštění
    
Zdrojové kódy jsou v hlavní složce společně se souborem Makefile.
Pro překlad a sestavení projektu tedy stačí použít příkaz:

```
make
```

následně se vyvtoří binární soubor ipk25chat-client, který lze spustit například:

```
./ipk25chat-client
```

Bez parametrů však vypíše pouze nápovědu.

### Parametry

Program poté podporuje tyto parametry:
```
Usage: ./chatClient -t <protocol> -s <hostname/IP> -p <port> -d <timeout> -r <retransmissions>
       protocol:          udp or tcp. (required)
       hostname:          server hostname or ip. (required)
       port:              server port. (optional)
       timeout:           UDP confirmation timeout in milliseconds. (optional)
       retransmissions:   maximum number of UDP retransmissions. (optional)

Default values for optional arguments:
       port:              4567
       timeout:           250ms
       retransmissions:   3
```

Z toho lze vyčíst parametry, které musí uživatel zadat. Těmi jsou <b>volba protokolu</b> a <b>IP adresa/název serveru</b>.

Dále volitelné parametry <b>hostitelského portu</b>, <b>délku času čekání na potvrzovací zprávu</b> a <b>počet dodatečných pokusů pro odeslání zprávy</b>.

Pokud uživatel nezadá volitelné parametry, použijí se výchozí, kterými jsou:
port:4567
timeout: 250ms
retransmisions:3

### Příkazy

Klient podoruje několik příkazů, které lze serveru zaslat nebo lokálně provést nějakou akci.

#### Lokální příkazy:
```
/rename {DisplayName} - changes the display name localy.
```

Při kterém se změní display name lokálně a pošle se v další zprávě serveru již aktualizovaný.

#### Serverové příkazy

```
/auth {Username} {Secret} {DisplayName} - Autorises to the server. (use once)
/join {ChannelID}                       - Changes the channel.
/rename {DisplayName}                   - Changes the display name.
/bye                                    - disconnects from the server.
{messageContent}                        - Sends message.
```

Tyto příkazy jsou zpracováný tímto klientem a následně odeslány serveru a to buď pomocí UDP nebo TCP, podle toho, který byl zvolen při spuštění programu.

## Teoretická část

### Přehled teoretických základů

Tento projekt pracuje nad transportními protokoly TCP a UDP, proto pro pochopení tohoto projektu je potřeba pochopení těchto protokolů a toho, jak fungují.

To však není v této dokumetnaci zahrnuto. K samostudiu lze využít následující odkazy:
TCP: https://datatracker.ietf.org/doc/html/rfc9293
UDP: https://datatracker.ietf.org/doc/html/rfc768

### Shrnutí principů implementace

Celý projekt je rozdělen do několika hlavních částí, které dohromady zajišťují správnou funkčnost klienta pro chatovací server využívající IPK25CHAT protokol. 
Níže je popsáno, jak jsou tyto části navzájem propojeny a jaká je jejich role v projektu.

### Hlavní program (main.cpp)

Tento soubor slouží jako vstupní bod aplikace. Zajišťuje inicializaci programu, načítání argumentů příkazové řádky, nastavení signal handlerů a vytvoření instance klienta (UDPChatClient nebo TCPChatClient) podle zadaného protokolu.

#### Funkce:

Parsování argumentů (s využitím funkcí z modulu utilit – parseArguments()).

Inicializace a konfigurace (případně nastavení globálních proměnných pro signal handling, jako je například socket pro TCP).

Vytvoření a spuštění interaktivní smyčky, ve které se zpracovávají uživatelské příkazy.

Spuštění samostatného vlákna pro naslouchání zpráv ze serveru (metoda startListener()).


### Pomocné funkce (utils.h / utils.cpp)

Modul utilit obsahuje pomocné funkce, které se opakovaně používají v celém projektu, ať už jde o parsování vstupů, validaci textových řetězců nebo zobrazení nápovědy.

#### Klíčové funkce:

<b>parseArguments(int argc, char argv[])</b>
Načítá argumenty příkazové řádky a vytváří strukturu Options s následujícími poli:
- protocol – požadovaný protokol (udp nebo tcp). (Povinné)
- hostname – název serveru či IP adresa. (Povinné)
- port – port serveru (volitelný, výchozí: 4567).
- timeout – délka čekání na potvrzení u UDP v milisekundách (volitelný, výchozí: 250ms).
- retry_count – maximální počet retransmisi u UDP (volitelný, výchozí: 3).

<b>split(const std::string &str)</b>
Rozděluje vstupní řetězec podle mezer a vrací vector tokenů.

<b>isValidString(), isPrintableChar(), isValidMessage()</b>
Tyto funkce ověřují, že dané řetězce odpovídají pravidlům (např. délka, povolené znaky).

<b>help()</b>
Vypisuje nápovědu pro exekuci programu.

<b>commandHelp()</b>
Vypisuje nápovědu pro uživatele za běhu.


### Klientská logika – ChatClient a její odvozené třídy

Klíčová část projektu je založena na dědičnosti. Celý chat klient je implementován jako abstraktní třída ChatClient, z níž jsou odvozeny konkrétní třídy UDPChatClient a TCPChatClient.

#### ChatClient (abstraktní třída)

Tato třída definuje společná rozhraní a datové členy používané pro komunikaci se serverem. Zahrnuje stav spojení (např. "start", "auth", "open", "end"), správu socketu, generování identifikátorů zpráv, a virtuální metody pro odesílání zpráv.

#### UDPChatClient

Tato třída implementuje komunikaci se serverem pomocí UDP. Specifické funkce zahrnují:
- Odesílání zpráv pomocí UDP a kontrolu potvrzení (confirmation) prostřednictvím retry mechanismu s timeouty.
- Validaci a parsování příchozích zpráv.
- Použití standardních kontejnerů (std::vector) pro uchovávání identifikátorů (replyIDs, confirmIDs).
- Mechanismus opakovaného odesílání

#### TCPChatClient

Tato třída implementuje komunikaci pomocí TCP, kde se využívá explicitní navázání spojení (connect()) a odesílání/příjem dat pomocí funkcí send()/recv().
Důležité implementační prvky této části jsou:
- <b>Case-insensitive zpracování</b>
- <b>Kontrola odpovědí</b>:
Mechanismus čekání na potvrzení či odpověď je implementován pomocí polling smyčky (maximálně 5 sekund, s periodickou kontrolou). Tato metoda umožňuje okamžité ukončení čekání, pokud je odpověď přijata před uplynutím timeoutu.
- <b>Signal Handling</b>:
Třída obsahuje statickou funkci pro zpracování SIGINT, která provede odeslání zprávy BYE a následný úklid prostřednictvím metody cleanup().

### UML diagramy / popis zajímavých částí kódu
Toto je jednoduchý diagram popisující třídy a dědičnost v rámci tohoto projektu. Atributy a metody lze nejsu uvedeny kvůli přehlednosti. Podrobný popis tříd včetně jejich atributů a metod lze nalézt v dokumentaci vygenerovanou pomocí Doxygen.

![class_diagram](/img/classChatClient__inherit__graph.png)

ChatClient obsahuje virtuální metody, které obě dědící třídy ovveride a implementují podle použitého protokolu. Zmíněnými virtuálními metodamy jsou:

- <b>~ChatClient ()</b> - destruktor, který je nutný implementovat v každé metodě zvlášť, jelikož TCP používá connect() a je potřeba ho řádně ukončit. U UDP nevyužito.
- <b>connectToServer ()</b> - Opět TCP využívá connect(). U UDP nevyužito.

U následujících je stejný důvod pro použití virtuálních metod. Tím důvodem je rozdílnost UDP a TCP co se týče tvorbě, odesílání a přijímání zpráv.
- <b>auth (const std::string &username, const std::string secret, const std::string &displayName)</b>
- <b>joinChannel (const std::string &channel)</b>
- <b>sendMessage (const std::string &message)</b>
- <b>sendError (const std::string &error)</b>
- <b>bye ()</b>
- <b>listen ()</b>
- <b>startListener ()</b>

## Testování

#### lokální
Lokální testování probíhalo pomocí programu Wireshark a programu Netcat. Toto lokální testování sloužilo k ověření správnosti sestavení a odesílaní zpráv klientem.

#### referenční server
Dále pro testování byl využit referenční server, který byl přiložen právě pro možnost testování svého klienta.

### Popis testovacího prostředí

### Testovací scénáře
- Co bylo testováno (hlavní funkcionalita, okrajové případy, robustnost, atd.).
- Důvody, proč byly tyto scénáře zvoleny.

### Metodika testování
- Jak byly testy prováděny (automatizované testy, ruční testy, simulace, atd.).

### Vstupy, očekávané a skutečné výstupy
- Přehled vstupů, co se očekávalo a co bylo skutečně získáno.


## Závěr

Shrnutí dosažených výsledků a případná doporučení pro budoucí rozšíření či zlepšení projektu.

## Přílohy (volitelné)

Přehled doplňujících materiálů (např. konfigurace testovacího prostředí, podrobnější diagramy, logy testů).Dokumentace projektu
 