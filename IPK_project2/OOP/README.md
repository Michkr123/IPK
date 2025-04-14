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

Implementace klienta je postavena na dvou hlavních variantách komunikace – pomocí UDP a TCP protokolů. Klient nejprve naváže spojení se serverem (v případě TCP provede explicitní connect(), u UDP se OS postará o automatické přidělení lokálního portu) a poté začíná zpracovávat uživatelské vstupy. Každý uživatelský příkaz (ať už jde o lokální příkaz, jako je změna zobrazeného jména, nebo serverový příkaz jako AUTH, JOIN, MSG, ERR, BYE či PING) je zpracován sekvenčně. To znamená, že klient přijme jeden příkaz, odešle odpovídající zprávu serveru a čeká, dokud nedostane potvrzení (CONFIRM) nebo odpověď (REPLY) ze serveru, než začne zpracovávat další uživatelský vstup.

Pro komunikaci se serverem jsou zprávy strukturovány pomocí pevné hlavičky, kde se posílají důležité informace jako typ zprávy a identifikátor zprávy. Identifikátory zpráv jsou při odesílání převedeny do síťového pořadí (big-endian) pomocí funkce htons() a po obdržení jsou konvertovány zpět do pořadí hostitelského systému (ntohs()), což zajišťuje konzistenci dat napříč různými platformami.

Implementace odesílání zpráv u protokolu UDP rovněž zahrnuje mechanismus pro opakované odesílání, pokud potvrzovací zpráva není obdržena do daného časového intervalu. Tento mechanismus kontroluje, zda byl obdržen správný identifikátor zprávy, a případně provádí další pokusy o odeslání. Díky tomu je zajištěna robustnost komunikace.


## Architektura a struktura zdrojového kódu

Projekt se skládá ze tří základních částí:

#### Hlavní program (main.cpp):
Tento soubor obsahuje vstupní bod aplikace. Spravuje inicializaci programu – parsování argumentů, zpracování signálů, vytvoření a inicializaci instance klienta a zprovoznění interaktivní smyčky, která čeká na uživatelské příkazy. Hlavní program pak předává uživatelské příkazy do příslušného klientského modulu a čeká na dokončení dané akce (odeslání zprávy a její potvrzení / odpovědi).

#### Pomocné funkce (utils.h / utils.cpp):
V této části jsou implementovány pomocné funkce, mezi které patří:
- Parsování argumentů příkazové řádky (funkce parseArguments(), návratová struktura Options).

- Validace vstupních řetězců (funkce isValidString(), isPrintableChar() a isValidMessage()).

- Zobrazení nápovědy.

#### Klientská logika 
##### třída ChatClient a její odvozené třídy (UDPChatClient a TCPChatClient):
Klíčovou částí projektu je abstraktní třída ChatClient, která definuje společná rozhraní a vlastnosti pro komunikaci se serverem (např. stav spojení, socket, identifikátory zpráv). Z ní jsou odvozeny konkrétní třídy UDPChatClient a TCPChatClient, které implementují specifické chování pro komunikaci přes UDP a TCP protokoly.

<b>ChatClient</b>: Zajišťuje sdílení společné logiky a dat, která jsou potřebná u obou variant komunikace, jako je sledování stavu spojení či správa socketu.

<b>UDPChatClient</b> a <b>TCPChatClient</b>: Tyto třídy implementují konkrétní metody pro odesílání zpráv, připojení ke službě, zpracování odezvy serveru a případné retry nebo potvrzovací mechanismy. Tím se dosahuje toho, že společná část je centralizována v ChatClient, a specifika daného transportního protokolu jsou řešena v odvozených třídách.

Toto rozdělení zajišťuje, že:

- Hlavní logika (main a utils) se stará o inicializaci a obsluhu vstupů,

- Klientská logika centralizuje kontrolu stavu a komunikaci se serverem,

- A díky dědičnosti se snadno udržuje konzistence společných funkcí při změně implementace transportního protokolu.


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
 