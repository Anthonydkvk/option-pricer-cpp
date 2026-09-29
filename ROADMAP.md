# ROADMAP — option-pricer-cpp

Une étape = une session Claude Code = un commit. Copie le **prompt** de l'étape dans Claude Code.
Coche la case quand l'étape est validée (tests verts + tu sais l'expliquer).

## Structure cible
```
option-pricer-cpp/
├── CMakeLists.txt
├── CLAUDE.md
├── ROADMAP.md
├── README.md
├── include/pricer/
│   ├── option.hpp          # OptionType, ExerciseStyle, MarketParams, Option
│   ├── normal.hpp          # N(x), n(x)
│   ├── black_scholes.hpp   # prix + grecques analytiques
│   ├── binomial.hpp        # arbre CRR
│   ├── monte_carlo.hpp     # MC GBM + erreur standard
│   └── greeks.hpp          # struct Greeks + différences finies génériques
├── src/                    # .cpp correspondants
├── tests/                  # un fichier de test par module (GoogleTest)
├── apps/pricer_cli.cpp     # démo en ligne de commande
├── python/
│   ├── reference.py        # implémentation de référence NumPy/SciPy
│   ├── compare.py          # compare C++ vs Python (précision + temps)
│   └── convergence.py      # graphe de convergence CRR/MC → BS (PNG pour le README)
└── .github/workflows/ci.yml
```

---

## ☑ Étape 0 — Initialisation
**Prompt :**
> Lis CLAUDE.md et ROADMAP.md. Étape 0 : initialise le dépôt git, crée l'arborescence, un
> `.gitignore` (build/, .venv/, __pycache__/, *.png sauf docs/), et un CMakeLists.txt racine
> (C++17, warnings stricts, bibliothèque `pricer` + exécutable `pricer_cli` + tests GoogleTest via
> FetchContent, activés avec `enable_testing()`). Ajoute un test factice qui passe pour vérifier la
> chaîne. Explique-moi le rôle de chaque bloc du CMakeLists avant de l'écrire.

**À savoir expliquer :** bibliothèque vs exécutable, `target_include_directories`, pourquoi FetchContent, `ctest`.

---

## ☑ Étape 1 — Modèle d'option + Black-Scholes
**Prompt :**
> Étape 1 : crée `option.hpp` (enum OptionType Call/Put, enum ExerciseStyle European/American,
> struct MarketParams {S, r, q, sigma}, struct Option {type, style, K, T}) avec validation des
> entrées, `normal.hpp` (N et n), puis `black_scholes.hpp/.cpp` : prix et grecques analytiques
> (struct Greeks {delta, gamma, vega, theta, rho}). Avant de coder, rappelle-moi les formules de d1,
> d2, du prix et de chaque grecque, avec leur intuition. Tests : valeurs de référence de CLAUDE.md,
> parité call-put, exceptions sur entrées invalides.

**À savoir expliquer :** d1/d2, pourquoi N(d2) = probabilité risque-neutre d'exercice, signe de Θ, parité call-put.

---

## ☑ Étape 2 — Arbre binomial CRR
**Prompt :**
> Étape 2 : implémente `binomial.hpp/.cpp` : arbre CRR (u = e^{σ√Δt}, d = 1/u,
> p = (e^{(r−q)Δt} − d)/(u − d)), backward induction avec un seul vecteur (mémoire O(N)),
> exercice anticipé pour les américaines. Vérifie que 0 < p < 1 sinon exception. Tests : convergence
> vers BS (erreur < 1e-2 à 500 pas et décroissante), put américain ≈ 6.0902 (5000 pas),
> américain ≥ européen, call américain sans dividende = européen, parité call-put en européen.
> Explique-moi pourquoi la convergence oscille (pair/impair).

**À savoir expliquer :** probabilité risque-neutre, backward induction, pourquoi on n'exerce jamais un call américain sans dividende, complexité O(N²) en temps / O(N) en mémoire.

---

## ☑ Étape 3 — Monte Carlo
**Prompt :**
> Étape 3 : implémente `monte_carlo.hpp/.cpp` pour les européennes : simulation exacte du GBM à
> maturité S_T = S·exp((r − q − σ²/2)T + σ√T·Z), actualisation, retour d'une struct
> {price, std_error, ci_low, ci_high}. Paramètres : nombre de trajectoires, seed, option variables
> antithétiques. Tests : |MC − BS| < 3 erreurs standard (500 000 trajectoires), l'erreur standard
> diminue en ~1/√N, les antithétiques réduisent l'erreur standard. Refuse les américaines
> (exception claire, avec un commentaire qui mentionne Longstaff-Schwartz comme extension possible).

**À savoir expliquer :** loi des grands nombres, vitesse 1/√N, variables antithétiques, pourquoi le MC standard ne marche pas pour l'américain.

---

## ☐ Étape 4 — Grecques par différences finies
**Prompt :**
> Étape 4 : implémente dans `greeks.hpp/.cpp` une fonction générique qui calcule Δ, Γ, Vega, Θ, ρ
> par différences finies centrées pour n'importe quelle méthode de pricing (passée en
> `std::function` ou template), avec les pas définis dans CLAUDE.md. Pour Monte Carlo, utilise les
> mêmes nombres aléatoires (même seed) pour tous les prix bumpés. Tests : différences finies BS ≈
> analytiques (1e-4 relatif), grecques CRR proches de BS pour une européenne, grecques d'un put
> américain cohérentes (Δ entre −1 et 0, Γ > 0).

**À savoir expliquer :** erreur de troncature vs erreur d'arrondi dans le choix du pas, pourquoi le Gamma MC est bruité, common random numbers.

---

## ☐ Étape 5 — Démo CLI
**Prompt :**
> Étape 5 : écris `apps/pricer_cli.cpp` qui prend les paramètres en arguments (avec des valeurs par
> défaut = cas standard de CLAUDE.md) et affiche un tableau aligné : pour chaque méthode, prix,
> grecques et temps de calcul (std::chrono). Ajoute une option `--csv` qui sort les mêmes résultats
> en CSV (utilisé par compare.py).

---

## ☐ Étape 6 — Référence Python + benchmark
**Prompt :**
> Étape 6 : dans `python/`, crée `reference.py` (BS avec scipy.stats.norm, CRR vectorisé NumPy,
> MC vectorisé NumPy, grecques), `compare.py` qui lance `pricer_cli --csv`, compare chaque valeur à
> la référence Python (écart absolu) et compare les temps d'exécution, et `convergence.py` qui trace
> l'erreur CRR et MC vs BS en fonction du nombre de pas/trajectoires (échelle log-log) dans
> `docs/convergence.png`. Ajoute un `pyproject.toml` (uv) avec numpy, scipy, matplotlib.

**À savoir expliquer :** pourquoi le C++ est plus rapide sur la boucle CRR, pourquoi NumPy vectorisé réduit l'écart.

---

## ☐ Étape 7 — README + CI
**Prompt :**
> Étape 7 : rédige un README.md en anglais, niveau recruteur quant : objectif, fonctionnalités,
> formules clés (LaTeX GitHub), architecture, commandes de build/test, tableau de résultats réels
> (issus de pricer_cli et compare.py, ne rien inventer), graphe de convergence, limites et extensions
> possibles (Longstaff-Schwartz, dividendes discrets, volatilité locale/Heston, parallélisation MC).
> Ajoute `.github/workflows/ci.yml` : build + ctest sur ubuntu-latest (GCC) et macos-latest (Clang),
> et un badge CI dans le README.

---

## Publication sur GitHub (depuis le Mac)
```bash
# 1. Crée un dépôt vide "option-pricer-cpp" sur github.com (sans README, sans .gitignore)
# 2. Dans le dossier du projet :
git remote add origin https://github.com/Anthonydkvk/option-pricer-cpp.git
git branch -M main
git push -u origin main
```
Ensuite, sur la page du dépôt : ajouter une description courte, des topics
(`cpp`, `quantitative-finance`, `option-pricing`, `black-scholes`, `monte-carlo`, `binomial-tree`),
et **épingler le dépôt** sur ton profil GitHub.

## Extensions (après, si le temps le permet)
- Monte Carlo américain par **Longstaff-Schwartz**.
- Volatilité implicite (Newton-Raphson + bissection de secours).
- Parallélisation du Monte Carlo (`std::thread` ou OpenMP) + benchmark.
- Schéma aux différences finies (EDP, Crank-Nicolson).