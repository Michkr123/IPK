# IPK project 1

Projekt je vypracovný v rámci předmětu IPK na VUT FIT.
Program prohledá vybrané porty zařízení na daném síťovém rozhraní a ty vypíše společně s jejich stavem na standardní výstup.

## Použití

Nejprve je potřeba spustit překlad:
```
Make
```

Formát spuštění s možnými parametry:
```
./ipk-l4-scan [-i interface | --interface interface] [--pu port-ranges | --pt port-ranges | -u port-ranges | -t port-ranges] {-w timeout} [domain-name | ip-address]
```
Příklad spuštění:
```
./ipk-l4-scan --interface eth0 -u 53,67 2001:67c:1220:809::93e5:917
./ipk-l4-scan -i eth0 -w 1000 -t 80,443,8080 www.vutbr.cz
```

Kompletní původní zadání lze vidět [zde](https://git.fit.vutbr.cz/NESFIT/IPK-Projects/src/branch/master/Project_1/omega) (dokud bude dostupné)

## Zdroje:
getopt https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Option-Example.html
UDP packets sending https://gist.github.com/jimfinnis/6823802
socket https://www.geeksforgeeks.org/socket-programming-in-cpp/
inet_pton https://man7.org/linux/man-pages/man3/inet_pton.3.html

