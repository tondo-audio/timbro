# TIMBRO
### Neural Amp Modeler Plugin for Logic Pro
**Product Requirements Document · v1.0 · Marzo 2026**

---

| Campo | Dettaglio |
|---|---|
| Nome prodotto | Timbro |
| Tipologia | Audio Unit (AU) Plugin per macOS |
| Target DAW | Logic Pro (uso personale) |
| Distribuzione | Open source · GitHub |
| Framework | JUCE 7 + NeuralAmpModelerCore |
| Versione PRD | 1.0 · Marzo 2026 |

---

## 1. Vision e Obiettivo

Timbro è un plugin AU open source per Logic Pro che porta il suono di amplificatori reali catturati con tecnologia Neural Amp Modeler (NAM) direttamente nella DAW, attraverso un'interfaccia radicalmente semplice: una sola manopola.

L'utente non deve scegliere profili, gestire file, configurare catene di segnale o conoscere la tecnologia NAM. Gira la manopola verso sinistra per suoni puliti, verso destra per suoni distorti. Il plugin fa tutto il resto.

---

## 2. Problema che Risolve

### 2.1 Il contesto attuale

I plugin NAM esistenti (NAM ufficiale, NiceStomps, RigBrowser) richiedono all'utente di:

- Cercare e scaricare profili da Tone3000 manualmente
- Capire la differenza tra LSTM, WaveNet, profili DI, Full Rig
- Gestire file .nam in cartelle locali
- Configurare IR cabinet separatamente
- Capire parametri tecnici come gain, presence, sample rate

### 2.2 Il chitarrista target

Un chitarrista che vuole suonare in Logic Pro con un suono di amp reale, senza diventare un esperto di amp modeling. Sa cosa vuole sentire — più pulito, più sporco — ma non vuole gestire tecnologia.

---

## 3. La Soluzione: Una Manopola

### 3.1 Concept centrale

Una singola manopola va da 0 a 10. Lo spazio è diviso in zone sonore con blend audio fluido tra una zona e l'altra. I profili NAM sono bundled nel plugin — nessun download, nessuna API, funziona offline.

| Posizione | Etichetta | Amp di riferimento | Cabinet IR |
|---|---|---|---|
| 0 – 2 | **CLEAN** | Fender Deluxe Reverb clean | Small open-back |
| 2 – 4 | **WARM** | Vox AC30 Top Boost | Vintage 2x12 |
| 4 – 6 | **CRUNCH** | Marshall 1959SLP Plexi pushed | Marshall 4x12 Greenback |
| 6 – 8 | **DRIVE** | Marshall JCM800 ch2 | Marshall 4x12 V30 |
| 8 – 10 | **LEAD** | Mesa Boogie Rectifier | Mesa 4x12 Modern |

### 3.2 Blend tra zone

Tra una zona e l'altra il plugin esegue un blend audio in tempo reale: fa girare entrambi i profili NAM adiacenti in parallelo e ne mixa l'output in proporzione alla posizione della manopola. La transizione è continua e impercettibile.

Esempio: manopola a 5.0 (metà tra CRUNCH e DRIVE) → 50% audio dal Plexi + 50% audio dal JCM800.

---

## 4. Interfaccia Utente

### 4.1 Principi di design

- Una sola manopola grande, centrale, dominante
- Estetica analogica anni 70 — nera, essenziale, fisica
- Nessun termine tecnico visibile all'utente
- Il nome della zona corrente appare sotto la manopola in grande
- Nessun menu, nessuna lista, nessun browser

### 4.2 Elementi UI

- **Manopola principale** — da 0 a 10, con tacche fisiche alle 5 zone
- **Label zona** — testo grande sotto la manopola (CLEAN / WARM / CRUNCH / DRIVE / LEAD)
- **VU meter** — solo decorativo/estetico, stile analogico
- **Volume IN/OUT** — due piccole manopole secondarie ai lati, meno prominenti
- **Bypass** — toggle on/off discreto

### 4.3 Cosa NON c'è nell'UI

- Nessun browser profili
- Nessun file picker
- Nessun EQ visibile
- Nessuna indicazione della tecnologia NAM
- Nessun termine come "IR", "cabinet", "sample rate"

---

## 5. Architettura Tecnica

### 5.1 Stack

- Framework plugin: JUCE 7 con CMake
- Engine NAM: NeuralAmpModelerCore (C++)
- Formato output: Audio Unit (AU) per macOS
- IR convolution: libreria built-in JUCE o custom FFT convolver
- Firma: nessuna (uso personale locale, Logic con flag non-signed AU)

### 5.2 Signal chain interna

```
Input audio
  → Noise gate
  → [NAM zona corrente + NAM zona adiacente] in parallelo con blend
  → IR convolution (cabinet bundled per zona)
  → EQ post fisso (non esposto)
  → Volume output
```

### 5.3 Profili bundled

I file `.nam` e `.wav` (IR) sono inclusi direttamente nel bundle del plugin AU, nella cartella `Resources`. Nessun download a runtime, nessuna dipendenza di rete. Il plugin funziona completamente offline.

### 5.4 Gestione del blend

Il plugin mantiene in memoria due modelli NAM contemporaneamente — quello della zona corrente e quello della zona adiacente verso cui si sta muovendo la manopola. L'audio viene processato da entrambi in parallelo su thread separati e mixato prima dell'IR.

### 5.5 Requisiti di sistema

- macOS 12 o superiore
- Apple Silicon (ARM64) o Intel x64
- Logic Pro qualsiasi versione recente
- RAM minima: 4GB
- CPU: qualsiasi Mac moderno — NAM LSTM è leggero

---

## 6. Requisiti Funzionali

### 6.1 Must have (v1.0)

- Manopola principale 0–10 con 5 zone sonore
- 5 profili NAM bundled — uno per zona
- Blend audio fluido tra zone adiacenti
- IR cabinet bundled per ogni zona
- Volume input e output
- Bypass on/off
- Funziona in Logic Pro senza firma
- Latenza < 5ms a 256 sample buffer

### 6.2 Nice to have (v1.1+)

- Noise gate con soglia regolabile
- Preset A/B per confronto rapido
- MIDI learn sulla manopola principale
- Supporto VST3 per DAW alternative
- Temi UI alternativi

---

## 7. Strategia Open Source

### 7.1 Licenza

MIT License — massima permissività, permette fork e uso commerciale.

### 7.2 Repository GitHub

- README con istruzioni di build per macOS
- CONTRIBUTING.md per chi vuole aggiungere zone/profili
- GitHub Actions per build automatica AU
- Release con binario AU pre-compilato scaricabile

### 7.3 Licenza dei profili NAM

I profili NAM su Tone3000 sono caricati dagli utenti con licenze permissive. Prima di includere un profilo nel bundle, verificare la licenza del singolo profilo e ottenere eventuale consenso dell'autore. Crediti degli autori inclusi nel README e nell'about del plugin.

---

## 8. Roadmap

| Fase | Nome | Contenuto |
|---|---|---|
| Fase 1 | Scaffold + Audio | Progetto JUCE + NeuralAmpModelerCore. Plugin AU che carica un profilo NAM e processa audio in Logic. |
| Fase 2 | Zone + Blend | 5 profili bundled. Manopola con zone. Blend audio parallelo tra profili adiacenti. |
| Fase 3 | IR + UI base | IR convolution bundled per zona. UI funzionale con manopola, label zona, bypass, volume. |
| Fase 4 | UI finale + Release | UI definitiva stile analogico. VU meter. GitHub release con binario AU. README completo. |

---

## 9. Domande Aperte

- Quali profili NAM specifici usare per ogni zona? → Da testare manualmente su Tone3000
- Il blend parallelo su M4 Pro è abbastanza leggero da girare a 128 sample buffer? → Da misurare in Fase 1
- JUCE OpenGL o Metal per la UI? → Da valutare su Logic su Apple Silicon
- Includere un noise gate esposto o lasciarlo fisso interno?
