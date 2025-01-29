# Systemy operacyjne - projekt 2024/2025

**Imię i nazwisko**: Rafał Paliwoda

**Numer albumu**: 147878

**Kierunek**: Informatyka (niestacjonarnie)

**Temat: Basen**

## Założenia projektowe

W pewnym miasteczku znajduje się kompleks basenów krytych dostępny w godzinach od Tp do Tk. W jego skład wchodzą:

- basen olimpijski
- basen rekreacyjny
- brodzik dla dzieci

Na każdym z nich może przebywać w danej chwili maksymalnie X1, X2 i X3 osób płacących. Osoba odwiedzająca centrum kupuje
bilet czasowy upoważniający ją do korzystania z dowolnego basenu zgodnie z regulaminem przez określony czas Ti.

Zasady korzystania z basenów:

- Tylko osoby pełnoletnie mogą korzystać z basenu olimpijskiego
- Dzieci poniżej 10 roku życia nie mogą przebywać na basenie bez opieki osoby dorosłej
- W brodziku mogą się kąpać jedynie dzieci do 5 roku życia i ich opiekunowie
- Dzieci do 3 roku życia muszą pływać w pieluchomajtkach
- Średnia wieku w basenie rekreacyjnym nie może być wyższa niż 40 lat

## Testy manualne

1. [Testy opisane w tym pliku](https://github.com/boriusz/so_projekt_basen/blob/main/tests.md)

## Ogólny opis kodu

Kod projektu składa się z kilku głównych komponentów:

- [`main.cpp`](https://github.com/boriusz/so_projekt_basen/blob/main/src/main.cpp): Główny plik programu inicjalizujący
  IPC, tworzący procesy ratowników, kasjera i klientów.
- [`client.cpp`](https://github.com/boriusz/so_projekt_basen/blob/main/src/client/client.cpp): Implementacja zachowania
  klientów (zakup biletu, wybór basenu, reakcja na sygnały).
- [`lifeguard.cpp`](https://github.com/boriusz/so_projekt_basen/blob/main/src/lifeguard/lifeguard.cpp): Obsługa
  ratowników nadzorujących baseny i wysyłających sygnały ewakuacji.
- [`cashier.cpp`](https://github.com/boriusz/so_projekt_basen/blob/main/src/cashier/cashier.cpp): System sprzedaży
  biletów i zarządzania kolejką.
- [`pool.cpp`](https://github.com/boriusz/so_projekt_basen/blob/main/src/pool/pool.cpp): Implementacja logiki basenów (
  wejścia/wyjścia, limity wiekowe).
- [
  `maintenance_manager.cpp`](https://github.com/boriusz/so_projekt_basen/blob/main/src/maintenance_manager/maintenance_manager.cpp):
  Zarządzanie konserwacją obiektu.
- [`ui_manager.cpp`](https://github.com/boriusz/so_projekt_basen/blob/main/src/ui_manager/ui_manager.cpp): Interfejs
  użytkownika i wizualizacja stanu systemu.

## Co udało się zrobić?

- Implementacja wszystkich głównych komponentów projektu zgodnie z założeniami
- System sprzedaży biletów z obsługą klientów VIP
- Kontrola limitów wiekowych i zasad korzystania z basenów
- Obsługa sygnałów ewakuacji i konserwacji
- System monitorowania w czasie rzeczywistym
- Implementacja mechanizmów IPC (semafory, pamięć współdzielona, kolejki komunikatów)
- Logowanie zdarzeń w czasie rzeczywistym

## Napotkane problemy

- Synchronizacja procesów przy ewakuacji basenów
- Gubione wiadomości z kolejki komunikatów Ratownik-Klient -> Przeniesienie na sockety
- Obsługa zależności między opiekunem a dzieckiem

## Elementy specjalne

- Kolorowy interfejs monitorowania stanu obiektu
- Osobny program monitorujący do podglądu stanu systemu

## Linki do istotnych fragmentów kodu

Linki do istotnych fragmentów kodu, które obrazują wymagane w projekcie użyte konstrukcje:

- Obsługa procesów i sygnałów
    - [Tworzenie procesów](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/main.cpp#L80)
    - [Tworzenie procesów](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/main.cpp#L94)
    - [Tworzenie procesów](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/main.cpp#L123)
    - [Tworzenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/cashier/cashier.cpp#L22)
    - [Tworzenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/client/client.cpp#L312-L314)
    - [Tworzenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/lifeguard/lifeguard.cpp#L21)
    - [Tworzenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/ui_manager/ui_manager.cpp#L125-L165)
    - [Tworzenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/main.cpp#L197)
    - [Łączenie wątków](https://github.com/boriusz/so_projekt_basen/blob/4ab556cd4ee491599a6d890d15202b316fee2ba1/src/main.cpp#L222-L224)
    - [Łączenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/ui_manager/ui_manager.h#L59-L61)
    - [Łączenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/monitor/monitor.cpp#L32-L34)
    - [Łączenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/cashier/cashier.h#L31-L33)
    - [Łączenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/client/client.cpp#L342-L350)
    - [Łączenie wątków](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/lifeguard/lifeguard.cpp#L31-L33)
    - [Pthread mutex](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/lifeguard/lifeguard.cpp#L173-L175)
    - [Pthread mutex](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/lifeguard/lifeguard.cpp#L195-L197)
    - [Pthread mutex](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/ui_manager/ui_manager.cpp#L49-L61)
    - [Pthread mutex - Pool scoped lock](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/pool/pool.h#L76-L82)
    - [Pthread mutex - Pool scoped lock](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/pool/pool.cpp#L79)
    - [Pthread mutex - Pool scoped lock](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/pool/pool.cpp#L153)
    - [Pthread mutex - Pool scoped lock](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/pool/pool.cpp#L184-L199)
    - [Pthread mutex destroy](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/pool/pool.cpp#L53-L54)
    - [Obsługa sygnałów](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/common/signal_handler.cpp#L60)
    - [Obsługa sygnałów](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/common/signal_handler.cpp#L94-L111)

- Komunikacja międzyprocesowa (przykładowe)
    - [Inicjalizacja IPC](https://github.com/boriusz/so_projekt_basen/blob/main/src/main.cpp#L20-L80)
    - [ftok](https://github.com/boriusz/so_projekt_basen/blob/8d1cf618a08bc7a4d33582fa6264c73ff6cda8b2/src/common/shared_memory.h#L88-L89)
    - [Semafory - semctl](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/main.cpp#L43)
    - [Semafory - semop](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/pool/pool.cpp#L67-L141)
    - [Kolejka komunikatów - msgrcv](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/cashier/cashier.cpp#L158)
    - [Kolejka komunikatów - msgsnd](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/cashier/cashier.cpp#L61)
    - [Kolejka komunikatów - msgctl](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/common/signal_handler.cpp#L32)
    - [Pamięć współdzielona - shmat, shmdt](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/cashier/cashier.cpp#L31-L43)
    - [Pamięć współdzielona - shmctl](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/common/signal_handler.cpp#L42)
    - [Sockety - setup](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/lifeguard/lifeguard.cpp#L42-L69)
    - [Sockety - accept](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/lifeguard/lifeguard.cpp#L260-L293)
    - [Sockety - connect](https://github.com/boriusz/so_projekt_basen/blob/b57d36a1ee0ddfc882f39364ddf15be943b23765/src/client/client.cpp#L102-L140)

---

### [Link do repozytorium](https://github.com/boriusz/so_projekt_basen)

### Komendy

- `cmake .` - generuje pliki make
- `make` - buduje aplikacje
- `make clean` - usuwa poprzedni build
- `./swimming_pool` - uruchamia główną aplikację
- `./monitor` - uruchamia program monitorujący