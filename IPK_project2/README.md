# IPK project 2 - client for a chat server 
## Úvod

Tento projekt implementuje klient pro chatovací server využívající IPK25CHAT protokol.

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
port: 4567
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
/err {MessageContent}                   - Sends error to the server.
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
- Case-insensitive zpracování 
- Kontrola odpovědí:
Mechanismus čekání na potvrzení či odpověď je implementován pomocí polling smyčky (maximálně 5 sekund, s periodickou kontrolou). Tato metoda umožňuje okamžité ukončení čekání, pokud je odpověď přijata před uplynutím timeoutu.

Signal Handling:
Třída obsahuje statickou funkci pro zpracování SIGINT, která provede odeslání zprávy BYE a následný úklid prostřednictvím metody cleanup().

### UML diagramy / popis zajímavých částí kódu
- UML diagram tříd, sekvenční diagram nebo jiný diagram znázorňující klíčovou funkcionalitu.
- Popis významných sekcí zdrojového kódu a jejich logiky.

## Testování

### Popis testovacího prostředí
- Hardwarové a softwarové specifikace (operační systém, verze, síťová topologie, použitý hardware).

### Testovací scénáře
- Co bylo testováno (hlavní funkcionalita, okrajové případy, robustnost, atd.).
- Důvody, proč byly tyto scénáře zvoleny.

### Metodika testování
- Jak byly testy prováděny (automatizované testy, ruční testy, simulace, atd.).

### Vstupy, očekávané a skutečné výstupy
- Přehled vstupů, co se očekávalo a co bylo skutečně získáno.

### Porovnání s jiným podobným nástrojem (pokud existuje)
- Srovnání s alternativními řešeními, popř. odůvodnění vlastního přístupu.

## Extra funkcionality

### Přehled rozšířených funkcí nad rámec zadání
Stručný popis funkcí a vlastností, které nad rámec standardu přináší váš projekt.

### Důvody implementace extra funkcionalit
Motivace a přínos jednotlivých rozšíření.

## Bibliografie a použité zdroje

### Seznam literatury a online zdrojů
- Uveďte všechny použité zdroje, odkazy, knihy, články, případně dokumentaci API.
- Citace v souladu s pokyny fakulty.

### Použité příklady a ukázky kódu
- Uveďte zdroje nebo licenci použitých útržků kódu a odkaz na autora.

## Závěr

Shrnutí dosažených výsledků a případná doporučení pro budoucí rozšíření či zlepšení projektu.

## Přílohy (volitelné)

Přehled doplňujících materiálů (např. konfigurace testovacího prostředí, podrobnější diagramy, logy testů).Dokumentace projektu
 