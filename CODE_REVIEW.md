# Code Review — Cyfrowe-us-ugi („Pogoda 98”)

## Prompt użytkownika

> przeprowadź code review i zapisz w pliku md wraz z moim promptem

## Metadane przeglądu

| Pozycja | Wartość |
|---|---|
| Data | 2026-10-09 |
| Repozytorium | `gklos2115/Cyfrowe-us-ugi` |
| Branch | `arena/5658d2d3-cyfrowe-us-ugi` (od `main` @ `a34531b`) |
| Zakres | pełna zawartość repozytorium: `README.md` + stan gita |
| Metoda | ręczny przegląd plików, struktury repozytorium i historii gita |

## TL;DR

Repozytorium zawiera **wyłącznie plik `README.md`** (jeden commit „initial commit:”). Cały kod źródłowy opisany w dokumentacji nie istnieje w repozytorium, więc instrukcje budowania, testów i Dockera są **niewykonalne**. Sama dokumentacja jest natomiast dobrze napisana, spójna i uczciwie opisuje ograniczenia (w tym bezpieczeństwa). Przegląd dotyczy więc dokumentacji i stanu repozytorium — pełne code review kodu będzie możliwe dopiero po dodaniu źródeł.

## Stan repozytorium

```
$ git ls-tree -r HEAD --name-only
README.md

$ git log --all --oneline
a34531b initial commit:
```

Jeden commit, jeden plik, brak gałęzi z kodem (lokalnie i na `origin`).

## Zestawienie ustaleń

| # | Waga | Ustalenie |
|---|---|---|
| 1 | 🔴 Krytyczna | Brak kodu źródłowego — repozytorium nie zawiera żadnego z plików opisanych w README |
| 2 | 🔴 Krytyczna | Brak `CMakeLists.txt` i `Dockerfile` — sekcje „CMake / Docker” nie mają czego budować |
| 3 | 🟠 Wysoka | Brak `.gitignore` — przy dodaniu kodu istnieje ryzyko wcommitowania `users.db` (konta użytkowników) i artefaktów |
| 4 | 🟠 Wysoka | Mechanizm kont opisany w README jest demonstracyjny (własny hash `sha256_simple`, kod aktywacji wyświetlany w aplikacji) — słusznie oznaczony w dokumentacji, ale wymaga migracji przed jakimkolwiek wdrożeniem |
| 5 | 🟡 Średnia | Brak licencji (`LICENSE`) i CI — niejasne warunki użycia kodu, brak automatycznej weryfikacji builda/testów |
| 6 | 🟢 Niska | Mieszane zakończenia linii w `README.md` — ostatni wiersz tabeli endpointów ma CRLF, reszta pliku LF |
| 7 | 🟢 Niska | Tabela endpointów nie obejmuje serwowania plików statycznych (`/`) — drobiazg dokumentacyjny |

## Ustalenie 1 i 2 (krytyczne): brak plików opisanych w README

README deklaruje kompletną aplikację C++17 i wymienia konkretne pliki, z których **żaden nie istnieje** w repozytorium:

| Plik z README | Sekcja, która go wymaga | Stan |
|---|---|---|
| `main.cpp` | „Pliki”, build MinGW, konta/serwer | ❌ brak |
| `http_client.h` | „Pliki” | ❌ brak |
| `weather.h` | „Pliki” | ❌ brak |
| `utils.h` | „Pliki”, sekcja o `sha256_simple` | ❌ brak |
| `tests.cpp` | „Testy”, „Pliki” | ❌ brak |
| `static/index.html`, `static/desktop.css`, `static/app.js` | „Pliki”, „Testy” (`node --check`) | ❌ brak |
| `deps/sqlite3.c` (+ nagłówki SQLite) | build MinGW/CMake | ❌ brak |
| `CMakeLists.txt` | `cmake -S . -B build`, `ctest` | ❌ brak |
| `Dockerfile` | `docker build -t pogoda98 .` | ❌ brak |

Konsekwencja: każdej komendy z README (`g++ …`, `cmake …`, `ctest …`, `docker build …`, `node --check static/app.js`) nie da się wykonać. Jeśli kod istnieje lokalnie u autora — **nie został wypchnięty**. To jedyna najważniejsza rzecz do naprawienia.

## Ustalenie 3 (wysoka): brak `.gitignore`

README opisuje plik `users.db` (baza kont) tworzony w katalogu roboczym oraz artefakty (`pogoda.exe`, `tests.exe`, `build/`). Bez `.gitignore` pierwszy lepszy `git add .` wcommituje bazę użytkowników (hashe haseł, e-maile) do publicznego repozytorium. Proponowany minimalny `.gitignore`:

```gitignore
users.db
build/
*.exe
*.o
```

## Ustalenie 4 (wysoka): bezpieczeństwo mechanizmu kont

Na podstawie samego README (kod niedostępny do weryfikacji):

**Plusy dokumentacji:**
- uczciwe, widoczne zastrzeżenie, że `sha256_simple` **nie jest** SHA-256 ani bezpiecznym hashem haseł,
- wprost zapisana rekomendacja migracji do Argon2id/bcrypt, bezpiecznego generatora sesji, wygasania sesji i limitowania prób logowania,
- ostrzeżenie „Nie należy używać tu haseł używanych w innych serwisach”.

**Ryzyka do zaadresowania przed wdrożeniem (zgodne zresztą z tym, co sam README sygnalizuje):**
1. Własny, nienazwany hash zamiast KDF — podatność na szybkie łamanie offline, jeśli `users.db` wycieknie.
2. Kod aktywacji wyświetlany w aplikacji zamiast e-mailem nie stanowi żadnego czynnika weryfikacji — to tylko demo-flow, nie „aktywacja konta” w sensie bezpieczeństwa.
3. Tokeny Bearer bez opisanej daty ważności, unieważniania po stronie serwera (poza `/api/logout`) i ograniczania prób logowania.
4. Endpointy pogodowe wymagają sesji — OK jako kontrola dostępu, ale warto upewnić się w kodzie, że walidacja tokena jest stała i nie da się jej obejść (np. przez kolejność obsługi ścieżek).

Gdy kod trafi do repozytorium, te punkty należy zweryfikować wiersz po wierszu — to będzie najważniejsza część właściwego code review.

## Ustalenie 5 (średnia): licencja i CI

- Brak `LICENSE` — przy publicznym repozytorium na GitHubie kod formalnie nie ma licencji (wszystkie prawa zastrzeżone przez autora). Warto dodać choćby MIT, jeśli projekt ma być otwarty.
- Brak CI (np. GitHub Actions) kompilującego na Linuxie (libcurl) i Windowsie (MinGW/WinHTTP) i uruchamiającego `tests.exe` — przy trzech ścieżkach budowania (MinGW, CMake, Docker) automatyczna weryfikacja szybko wyłapie rozjazdy między README a kodem.

## Ustalienia 6–7 (niskie): drobiazgi w README

- Ostatni wiersz pliku (`| GET | /api/weather/7timer | … |`) kończy się `CRLF`, podczas gdy reszta pliku używa `LF`. Warto ujednolicić (np. `dos2unix` albo ustawienie `* text=auto eol=lf` w `.gitattributes`).
- Tabela endpointów opisuje samo API; można dodać wiersz o serwowaniu `static/` pod `/`, żeby tabela oddawała pełne zachowanie serwera.
- Drobna literówka do sprawdzenia przy okazji: spójność nazw („Pogoda 98” vs `pogoda98` w Dockerze vs `pogoda.exe`) — niespójność nie szkodzi, ale ujednolicenie ułatwi wyszukiwanie.

## Co w README zasługuje na pochwałę

Niezależnie od braków repozytorium, sama dokumentacja jest ponadprzeciętnie dobra jak na projekt tej klasy:

- **Jasna struktura** — źródła danych, UX, konta, build, testy, pliki, endpointy; wszystko w jednym miejscu.
- **Uczciwość co do ograniczeń** — limity darmowych serwisów, informacja o cache (5 min / 128 wpisów), jawne oznaczenie zastępowania Open-Meteo przez wttr.in przy awarii.
- **Dobre decyzje projektowe opisane wprost**: te same współrzędne dla trzech serwisów, „Nieznane miasto nie zamienia się na Warszawę” (czyli naprawiony klasyczny błąd fallbacku), anulowanie poprzednich requestów przy zmianie miasta (brak wyścigu odpowiedzi), niezależność awarii źródeł, obsłużone stany błędów (brak sieci, brak wyników, timeout, limit, niedostępność dostawcy), brak podstawiania losowych/zerowych danych za braki pomiarów.
- **Poprawne instrukcje budowania** dla trzech ścieżek (MinGW, CMake, Docker) — sensowne flagi, poprawne linkowanie (`-lwinhttp -lws2_32`), uwaga o `PORT` i katalogu `static`.
- **Sekcja bezpieczeństwa kont napisana tak, jak powinna** — bez udawania, że demo jest produkcyjne.

## Rekomendacje — kolejność działań

1. **Wypchnąć kod źródłowy** do repozytorium (`main.cpp`, `http_client.h`, `weather.h`, `utils.h`, `tests.cpp`, `static/*`, `deps/sqlite3.*`, `CMakeLists.txt`, `Dockerfile`) — bez tego projekt nie istnieje w repozytorium.
2. Dodać `.gitignore` **przed** pierwszym commitem z kodem (żeby nie wciągnąć `users.db`).
3. Zweryfikować w CI, że wszystkie komendy z README faktycznie działają (Linux: CMake + ctest; Windows: MinGW).
4. Dodać `LICENSE`.
5. Po dodaniu kodu — pełny przegląd bezpieczeństwa: hashowanie haseł (Argon2id), generacja i wygasanie sesji (CSPRNG), rate limiting logowania, walidacja wejść (`lat`, `lon`, `city`, `name`) pod kątem iniekcji (SQL w SQLite, nagłówki/ścieżki w serwerze HTTP — w tym path traversal przy serwowaniu `static/`), limity rozmiaru odpowiedzi po stronie klienta HTTP.
6. Ujednolicić zakończenia linii w `README.md` (usunięcie pojedynczego CRLF).

## Podsumowanie oceny

| Aspekt | Ocena | Komentarz |
|---|---|---|
| Kod źródłowy | ⛔ brak | Nie ma czego recenzować — repozytorium zawiera tylko README |
| Dokumentacja (README) | ✅ dobra | Spójna, uczciwa, kompletna informacyjnie |
| Stan repozytorium / higiena | ❌ słaby | 1 commit, brak .gitignore, licencji, CI |
| Bezpieczeństwo (wg opisu) | ⚠️ demo | Świadomie demonstracyjne; dobrze udokumentowane, wymaga pełnej migracji przed wdrożeniem |
| **Ocena łączna** | **Projekt niekompletny w repozytorium** | Najpierw kod do gita, potem właściwe code review |

---
*Przegląd wykonany automatycznie na zlecenie użytkownika; obejmuje wyłącznie to, co znajduje się w repozytorium na branchu `arena/5658d2d3-cyfrowe-us-ugi` w dniu 2026-10-09.*
