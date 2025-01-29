# Test 1: System VIP i kolejkowanie

Test sprawdza, czy system prawidłowo obsługuje priorytetyzację klientów VIP w kolejce oraz czy zachowywana jest kolejność FIFO dla klientów tego samego typu
oraz zachowanie przy pełnej kolejce

### Założenia
- VIP powinien zostać umieszczony na początku kolejki (przed klientami standardowymi)
- Między VIPami powinna być zachowana kolejność FIFO
- Między klientami standardowymi powinna być zachowana kolejność FIFO
- VIP nie powinien "przeskakiwać" innych VIPów
- Gdy kolejka zostanie zapełniona, nowy potencjalny klient rezygnuje z wejścia na obiekt

### Przebieg testu

Używam domyślnej konfiguracji symulacji, zmniejszając przy tym interwał tworzenia nowych klientów

#### Wycinek logów z przebiegu testu:

Error nie jest tutaj niczym złym, to tylko pokazanie, że klient jest
świadomy pełnej kolejki i opuszcza obiekt

```asm
Error in addToQueue: Queue is full
Error adding client to queue: Queue is full
Error in waitForTicket: Facility queue is full, client abandoning pool
Error in client 123: Facility queue is full, client abandoning pool
Client 51 trying to enter poolOlympic
Client 51 connected to pool Olympic
Client 51 added to pool Olympic
Error in addToQueue: Queue is full
Error adding client to queue: Queue is full
Error in waitForTicket: Facility queue is full, client abandoning pool
Error in client 124: Facility queue is full, client abandoning pool
Error in addToQueue: Queue is full
Error adding client to queue: Queue is full
Error in waitForTicket: Facility queue is full, client abandoning pool
Error in client 125: Facility queue is full, client abandoning pool
Error in addToQueue: Queue is full
Error adding client to queue: Queue is full
Error in waitForTicket: Facility queue is full, client abandoning pool
Error in client 126: Facility queue is full, client abandoning pool
Error in addToQueue: Queue is full
Error adding client to queue: Queue is full
Error in waitForTicket: Facility queue is full, client abandoning pool
Error in client 127: Facility queue is full, client abandoning pool
```

#### oraz podgląd kolejki:

```asm
Entrance Queue
Queue size: 99/100
 - Client 54 (VIP)
 - Client 56 (VIP)
 - Client 58 (VIP)
 - Client 61 (VIP)
 - Client 63 (VIP)
 - Client 65 (VIP)
 - Client 67 (VIP)
 - Client 70 (VIP)
 - Client 72 (VIP)
 - Client 77 (VIP)
 - Client 79 (VIP)
 - Client 4
 - Client 6
 - Client 7
 - Client 8
 - Client 9
 - Client 10
 - Client 11
 - Client 12
 - Client 13
 - Client 14
 - Client 15
 - Client 16
 - Client 18
 - Client 20
 - Client 21
 - Client 22
 - Client 23
 - Client 24
 - Client 25
 - Client 26
 - Client 27
 - Client 28
 - Client 29
 - Client 30
 - Client 32
 - Client 34
 - Client 35
 ...itd
```

### Wynik testu

Pozytywny, kolejka działa zgodnie z założeniami:
1. VIPy (3 i 5) zostali umieszczeni na początku kolejki
2. Między VIPami zachowana została kolejność FIFO (3 przed 5)
3. Klienci standardowi zachowali swoją oryginalną kolejność (1, 2, 4)
4. System prawidłowo rozróżnia i priorytetyzuje klientów VIP
5. Klienci którzy napotkają pełną kolejkę rezygnują z wejścia na obiekt

# Test 2: Regulamin basenów i ograniczenia wiekowe

_Test sprawdza przestrzeganie ograniczeń wiekowych dla poszczególnych basenów oraz zasad dotyczących opiekunów._

### Założenia
- Dzieci poniżej 10 roku życia muszą mieć opiekuna
- W brodziku mogą być tylko dzieci do 5 roku życia i ich opiekunowie
- Średnia wieku w basenie rekreacyjnym nie może przekroczyć 40 lat

### Przebieg testu

```asm
Error creating Client: Child under 10 needs a guardian
Error in client process: Child under 10 needs a guardian
Error creating Client: Child under 10 needs a guardian
Error in client process: Child under 10 needs a guardian
Klient 1 w wieku 17 wszedł na basen Recreational
Klient 2 w wieku 40 oraz dziecko 3 w wieku 1 weszli na basen Children
Klient 4 w wieku 62 oraz dziecko 5 w wieku 9 weszli na basen Recreational
Klient 4 w wieku 62 wszedł na basen Recreational
Klient 6 w wieku 52 wszedł na basen Olympic
Klient 7 w wieku 59 wszedł na basen Olympic
Klient 21 w wieku 37 wszedł na basen Olympic
Klient 14 w wieku 48 wszedł na basen Olympic
Klient 20 w wieku 30 wszedł na basen Olympic
Klient 6 w wieku 52 wszedł na basen Olympic
Klient 7 w wieku 59 wszedł na basen Olympic
Klient 8 w wieku 66 wszedł na basen Olympic
Klient 10 w wieku 20 wszedł na basen Olympic
Klient 12 w wieku 34 wszedł na basen Olympic
Klient 11 w wieku 27 wszedł na basen Olympic
Klient 15 w wieku 55 wszedł na basen Olympic
Klient 22 w wieku 44 wszedł na basen Olympic
Klient 23 w wieku 51 wszedł na basen Olympic
Klient 13 w wieku 41 wszedł na basen Olympic
Klient 4 podwyższył by średnią wieku poza limit (62 > 40)
Klient 9 w wieku 13 wszedł na basen Recreational
Klient 16 w wieku 38 oraz dziecko 17 w wieku 6 weszli na basen Recreational
Klient 16 w wieku 38 wszedł na basen Recreational
Klient 49 w wieku 53 wszedł na basen Olympic
Klient 1 w wieku 17 wszedł na basen Recreational
Klient 18 w wieku 60 oraz dziecko 19 w wieku 6 weszli na basen Recreational
Klient 18 w wieku 60 wszedł na basen Recreational
Klient 24 w wieku 58 wszedł na basen Olympic
Klient 51 w wieku 67 wszedł na basen Olympic
```

### Wynik testu

Pozytywny, wszystkie ograniczenia wiekowe i zasady regulaminu są prawidłowo egzekwowane:
1. Osoba niepełnoletnia nie mogła wejść na basen olimpijski
2. Dziecko bez opiekuna nie zostało wpuszczone (to wymagało dodatkowej zmiany warunków przy tworzeniu klientów)
3. Dziecko z opiekunem zostało wpuszczone do brodzika
4. Starsze dziecko nie zostało wpuszczone do brodzika a do basenu rekreacyjnego
5. System prawidłowo kontroluje średnią wieku na basenie rekreacyjnym

# Test 3: Procedura ewakuacji

_Test sprawdza poprawność działania systemu podczas ewakuacji basenu przez ratownika._

### Założenia
- Po sygnale ewakuacyjnym wszyscy klienci muszą opuścić basen
- Podczas ewakuacji nikt nie może wejść do basenu
- System musi prawidłowo zaktualizować stan basenu po ewakuacji
- Klienci mogą wrócić do basenu po sygnale powrotu

### Przebieg testu

Scenariusz testowy:
1. Zapełnienie basenu klientami
2. Wywołanie sygnału ewakuacji przez ratownika
3. Próba wejścia nowych klientów podczas ewakuacji

`./swimming_pool`
```asm
Klient 1 w wieku 29 oraz dziecko 2 w wieku 6 weszli na basen Recreational
Klient 1 w wieku 29 wszedł na basen Recreational
Klient 3 w wieku 51 oraz dziecko 4 w wieku 5 weszli na basen Children
Klient 5 w wieku 45 wszedł na basen Recreational
Klient 6 w wieku 52 wszedł na basen Olympic
Klient 7 w wieku 59 wszedł na basen Olympic
Klient 8 w wieku 66 wszedł na basen Recreational
Klient 9 w wieku 13 wszedł na basen Recreational
Klient 10 w wieku 20 wszedł na basen Recreational
Ratownik: Ewakuacja basenu Recreational!
Klient 9 otrzymał sygnał do ewakuacji z basenu Recreational
Próba wejścia na zamknięty basen Recreational - odmowa!
Klient 10 otrzymał sygnał do ewakuacji z basenu Recreational
Klient 1 otrzymał sygnał do ewakuacji z basenu Recreational
Klient 5 otrzymał sygnał do ewakuacji z basenu Recreational
Klient 8 otrzymał sygnał do ewakuacji z basenu Recreational
Próba wejścia na zamknięty basen Recreational - odmowa!
Klient 5 w wieku 45 wszedł na basen Olympic
Klient 11 w wieku 27 wszedł na basen Olympic
Klient 10 w wieku 20 wszedł na basen Olympic
Próba wejścia na zamknięty basen Recreational - odmowa!
```

`./monitor`
```asm
Swimming Pool Monitor - 

Status: OPEN

Olympic Pool
Occupancy: 5/100
Status: OPEN
Clients:
 - Client 6 (Age: 52)
 - Client 7 (Age: 59)
 - Client 5 (Age: 45)
 - Client 11 (Age: 27)
 - Client 10 (Age: 20)

--------------------------------------------------
Recreational Pool
Occupancy: 0/40
Status: CLOSED
Clients:

--------------------------------------------------
Children's Pool
Occupancy: 2/20
Status: OPEN
Clients:
 - Client 3 (Age: 51)
 - Client 4 (Age: 5, Has Guardian #3)

--------------------------------------------------
Entrance Queue
Queue size: 11/100
 - Client 12
 - Client 13
 - Client 14
 - Client 15
 - Client 16
 - Client 18
 - Client 20
 - Client 21
 - Client 22
 - Client 23
 - Client 24
```

### Wynik testu

Pozytywny, System ewakuacji działa zgodnie z wymaganiami:
1. Wszyscy klienci opuścili basen po sygnale ewakuacyjnym
2. Nowi klienci nie mogli wejść podczas ewakuacji
3. Stan basenu został prawidłowo zaktualizowany
4. Po sygnale powrotu basen został ponownie otwarty i można było do niego wejść

# Test 4: Procedura konserwacji

_Test sprawdza działanie systemu podczas okresowej konserwacji i wymiany wody._

### Założenia
- Podczas konserwacji wszystkie baseny muszą być puste
- System musi poczekać na opuszczenie obiektu przez wszystkich klientów
- Żaden nowy klient nie może wejść podczas konserwacji
- Po zakończeniu konserwacji obiekt wraca do normalnego funkcjonowania

### Przebieg testu

Scenariusz testowy:
1. Normalne funkcjonowanie obiektu
2. Rozpoczęcie procedury konserwacji
3. Oczekiwanie na opuszczenie obiektu przez klientów
4. Próby wejścia podczas konserwacji
5. Zakończenie konserwacji

`./swimming_pool`
```asm
Klient 1 w wieku 29 oraz dziecko 2 w wieku 6 weszli na basen Recreational
Klient 1 w wieku 29 wszedł na basen Recreational
Klient 3 w wieku 51 oraz dziecko 4 w wieku 5 weszli na basen Children
Klient 5 w wieku 45 wszedł na basen Recreational
Klient 6 w wieku 52 wszedł na basen Olympic
Rozpoczyna się przerwa techniczna. Klienci są proszenie o opuszczenie
Klient szukający basenu opuszcza obiekt ze względu na przerwę techniczną
Zamykamy basen!
Ratownik: zamykanie basenu Olympic z powodu nadchodzącej konserwacji
Zamykamy basen!
Ratownik: zamykanie basenu Recreational z powodu nadchodzącej konserwacji
Zamykamy basen!
Ratownik: zamykanie basenu Children z powodu nadchodzącej konserwacji
Klient: Basen jest w trybie konserwacji, opuszczam obiekt
Klient: Basen jest w trybie konserwacji, opuszczam obiekt
Klient: Basen jest w trybie konserwacji, opuszczam obiekt
Klient: Basen jest w trybie konserwacji, opuszczam obiekt
Klienci opuścili obiekt, rozpoczynamy przerwę techiczną.
Klient szukający basenu opuszcza obiekt ze względu na przerwę techniczną
Klient szukający basenu opuszcza obiekt ze względu na przerwę techniczną
Klient szukający basenu opuszcza obiekt ze względu na przerwę techniczną
Klient szukający basenu opuszcza obiekt ze względu na przerwę techniczną
Klient szukający basenu opuszcza obiekt ze względu na przerwę techniczną
Koniec przerwy technicznej
Ratownik: Ponowne otwarcie Children!
Ratownik: Ponowne otwarcie Recreational!
Ratownik: Ponowne otwarcie Olympic!
Klient 13 w wieku 41 wszedł na basen Olympic
Klient 14 w wieku 48 wszedł na basen Olympic
Ratownik: Ewakuacja basenu Olympic!
Próba wejścia na zamknięty basen Olympic - odmowa!
Klient 13 otrzymał sygnał do ewakuacji z basenu Olympic
Klient 14 otrzymał sygnał do ewakuacji z basenu Olympic
Próba wejścia na zamknięty basen Olympic - odmowa!
Klient 13 podwyższył by średnią wieku poza limit (41 > 40)
Klient 16 w wieku 38 oraz dziecko 17 w wieku 6 weszli na basen Recreational
Klient 16 w wieku 38 wszedł na basen Recreational
Próba wejścia na zamknięty basen Olympic - odmowa!
Próba wejścia na zamknięty basen Olympic - odmowa!
Klient 13 w wieku 41 wszedł na basen Recreational
```

`./monitor`
```asm
Swimming Pool Monitor - 

Status: CLOSED

Olympic Pool
Occupancy: 0/100
Status: CLOSED
MAINTENANCE IN PROGRESS
Clients:

--------------------------------------------------
Recreational Pool
Occupancy: 0/40
Status: CLOSED
MAINTENANCE IN PROGRESS
Clients:

--------------------------------------------------
Children's Pool
Occupancy: 0/20
Status: CLOSED
MAINTENANCE IN PROGRESS
Clients:
```

### Wynik testu

Pozytywny! System konserwacji działa prawidłowo:
1. Wszyscy klienci opuścili obiekt przed rozpoczęciem konserwacji
2. Nowi klienci nie mogli wejść podczas konserwacji
3. System prawidłowo poczekał na opuszczenie obiektu
4. Po zakończeniu konserwacji obiekt wrócił do normalnego funkcjonowania