# Pogoda 98

Lokalna aplikacja pogodowa C++17 z pulpitem inspirowanym Windows 95/98: klasyczne szare okna, menu Start, pasek zadań, przeciąganie, minimalizacja i maksymalizacja. Zawiera Saper, Węża i Notatnik. Gry są dostępne bez konta.

## Trzy źródła pogody

1. **Open-Meteo** — https://open-meteo.com/ — pogoda bieżąca, prognoza 7 dni i geokodowanie miejscowości (GeoNames). Darmowy endpoint jest przeznaczony do użytku niekomercyjnego, w limitach dostawcy (https://open-meteo.com/en/pricing).
2. **wttr.in** — https://github.com/chubin/wttr.in — dodatkowy odczyt temperatury, odczuwalnej, wilgotności, wiatru i ciśnienia, format JSON j1.
3. **7Timer!** — https://www.7timer.info/doc.php?lang=en — prognoza CIVIL co 3 godziny. Prędkość wiatru jest kategorią: interfejs przelicza ją na przedział km/h, a termin prognozy z init (UTC) + timepoint na czas lokalny miejscowości.

Nie są potrzebne klucze API. Darmowe serwisy mogą mieć limity, opóźnienia i przerwy. Aplikacja pokazuje rzeczywisty stan każdego źródła i umożliwia ponowienie nieudanego pobrania. Nie podstawia losowych danych ani zer za brakujące pomiary.

## Pogoda

- Zaloguj się, wpisz miasto, kliknij Szukaj i wybierz właściwy wynik (region i kraj).
- Wszystkie trzy serwisy dostają te same współrzędne. Nieznane miasto nie zamienia się na Warszawę.
- Trzy połączenia są niezależne; awaria jednego źródła nie blokuje pozostałych.
- Open-Meteo jest podstawowym źródłem warunków bieżących; gdy zawiedzie, używany jest wttr.in z czytelnym oznaczeniem.
- Wyniki źródeł są buforowane na serwerze przez 5 minut (maksymalnie 128 wpisów). Odświeżenie w tym okresie może zwrócić ten sam pomiar.
- Zmiana miasta anuluje poprzednie pobieranie po stronie interfejsu; spóźnione odpowiedzi nie nadpisują nowego widoku.
- Widok obsługuje brak sieci, brak wyników wyszukiwania, brak pomiarów, błędną sesję, limit czasu i niedostępność dostawcy.

## Konto lokalne

Rejestracja -> kod aktywacji wyświetlony w aplikacji -> aktywacja -> logowanie. E-mail nie jest wysyłany. Baza users.db zachowuje dotychczasowe konta. Pogodowe endpointy wymagają Bearer Token.

**Ograniczenie istniejącego systemu kont:** jest to demonstracyjny mechanizm logowania. Funkcja sha256_simple w utils.h jest własnym hashem, a nie SHA-256 ani bezpiecznym hashem haseł. Przed publicznym wdrożeniem potrzebna jest migracja do Argon2id/bcrypt, bezpieczny generator sesji, wygasanie sesji i ograniczanie prób logowania. Nie należy używać tu haseł używanych w innych serwisach. Zmiany interfejsu nie stanowią audytu bezpieczeństwa produkcyjnego.

## Windows / MinGW

```powershell
gcc -c -O2 deps/sqlite3.c -o deps/sqlite3.o
g++ -std=c++17 -O2 -I deps main.cpp deps/sqlite3.o -o pogoda.exe -lwinhttp -lws2_32
.\pogoda.exe
```

Uruchamiaj z katalogu projektu. Adres: http://localhost:8080. Opcjonalna zmienna PORT zmienia port. Katalog static musi znajdować się w katalogu roboczym serwera.

## CMake / Docker

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Windows używa WinHTTP, Linux libcurl. SQLite i biblioteki nagłówkowe są dołączone do projektu; CMake nie pobiera ich z sieci.

```sh
docker build -t pogoda98 .
docker run --rm -p 8080:8080 pogoda98
```

## Testy

```powershell
g++ -std=c++17 -O2 tests.cpp -o tests.exe
.\tests.exe
node --check static/app.js
```

## Pliki

- main.cpp — konta, sesje, serwer i statyczne pliki.
- http_client.h — klient HTTPS dla Windows/Linux, limity połączeń i rozmiaru odpowiedzi, sprawdzanie statusu HTTP.
- weather.h — trzy integracje, geokodowanie, walidacja współrzędnych i cache.
- static/index.html, static/desktop.css, static/app.js — pulpit, pogoda, konta i gry.
- utils.h, tests.cpp — dotychczasowe funkcje pomocnicze i testy.

## Endpointy

| Metoda | Ścieżka | Parametry |
|---|---|---|
| POST | /api/register | username, email, password |
| POST | /api/confirm | code |
| POST | /api/login | username, password |
| POST | /api/logout | Bearer Token |
| GET | /api/locations | name; wymaga sesji |
| GET | /api/weather/openmeteo | lat, lon, city; wymaga sesji |
| GET | /api/weather/wttr | lat, lon, city; wymaga sesji |
| GET | /api/weather/7timer | lat, lon, city; wymaga sesji |
