# CLAUDE.md — option-pricer-cpp

## Contexte du projet
Pricer d'options en C++ moderne, construit comme projet de portfolio pour des candidatures
**Quant Developer / Financial Engineering** (Murex, banques CIB). Le code doit être :
- **correct** (validé par des tests et des valeurs de référence connues),
- **lisible** (un recruteur doit comprendre l'architecture en 2 minutes),
- **explicable** : l'auteur (Anthony) doit pouvoir justifier chaque choix en entretien.

L'auteur est étudiant ingénieur (finance de marché), à l'aise en Python, en apprentissage
avancé du C++. **Explique chaque décision en français simple avant de coder**, et avance
**étape par étape** (voir `ROADMAP.md`) : une étape = un commit, validée avant de passer à la suite.

## Périmètre fonctionnel
- Options **européennes et américaines**, **Call et Put**, sous-jacent sans dividende
  (dividende continu `q` prévu dans la structure, défaut 0).
- Trois méthodes de pricing :
  1. **Black-Scholes** formule fermée (européennes uniquement).
  2. **Arbre binomial CRR** (européennes et américaines, exercice anticipé).
  3. **Monte Carlo** GBM (européennes), avec erreur standard et intervalle de confiance à 95 %.
- **Grecques** : Δ, Γ, Vega (ν), Θ, ρ
  - analytiques pour Black-Scholes,
  - par **différences finies centrées** pour toutes les méthodes (bump & reprice).
- **Référence Python** (NumPy/SciPy) pour valider les résultats et comparer les temps d'exécution.

## Stack et conventions techniques
- **C++17**, **CMake ≥ 3.20**, compilateurs GCC/Clang, warnings stricts :
  `-Wall -Wextra -Wpedantic -Werror` (hors dépendances).
- **Tests : GoogleTest**, récupéré via `FetchContent` (aucune installation manuelle).
- Aucune autre dépendance externe en C++ (pas de Boost, pas d'Eigen).
- Loi normale : `N(x) = 0.5 * std::erfc(-x / std::sqrt(2.0))`, densité codée à la main.
- Aléatoire : `std::mt19937_64` + `std::normal_distribution<double>`, **seed explicite** en paramètre
  (résultats reproductibles).
- Namespace unique : `pricer`.
- En-têtes dans `include/pricer/`, implémentations dans `src/`, `#pragma once`.
- Style : `snake_case` pour fonctions/variables, `PascalCase` pour types, `const` et références partout
  où c'est possible, pas de `using namespace std;` dans les en-têtes.
- Entrées invalides (σ ≤ 0, T ≤ 0, S ≤ 0, K ≤ 0, nombre de pas ≤ 0) → `std::invalid_argument`.
- Pas d'optimisation prématurée : d'abord correct et lisible, ensuite mesuré, ensuite optimisé.

## Conventions financières (à respecter partout)
- Paramètres : `S` spot, `K` strike, `r` taux sans risque continu, `q` dividende continu,
  `sigma` volatilité annuelle, `T` maturité en années.
- **Vega** et **ρ** exprimés **pour une variation de 1.0** (100 %) de σ et de r
  (préciser dans le README comment convertir en « par 1 % »).
- **Θ** exprimé **par an** (dérivée du prix par rapport au temps calendaire, valeur négative pour un call ATM).
- Différences finies centrées, pas relatifs : `h_S = 1e-4 * S`, `h_sigma = 1e-4`, `h_r = 1e-4`, `h_T = 1e-4`.
  Gamma : différence seconde centrée sur S.
- Monte Carlo : utiliser **les mêmes nombres aléatoires** (même seed) pour le prix de base et les prix
  bumpés (common random numbers), sinon les grecques MC sont inutilisables.

## Valeurs de référence (tests)
Cas standard : `S=100, K=100, r=0.05, q=0, sigma=0.2, T=1`.

| Quantité | Valeur attendue | Tolérance |
|---|---|---|
| Call européen BS | 10.450584 | 1e-6 |
| Put européen BS | 5.573526 | 1e-6 |
| Δ call | 0.636831 | 1e-6 |
| Γ | 0.018762 | 1e-6 |
| Vega | 37.524035 | 1e-5 |
| Θ call (par an) | -6.414028 | 1e-5 |
| ρ call | 53.232482 | 1e-5 |
| Put américain (CRR, 5000 pas) | ≈ 6.0902 | 1e-3 |

Autres tests obligatoires :
- **Parité call-put** : `C - P = S·e^{-qT} - K·e^{-rT}` (BS et CRR européen).
- **Convergence CRR → BS** : erreur < 1e-2 à 500 pas, décroissante quand le nombre de pas augmente.
- **Américain ≥ européen** ; **call américain sans dividende = call européen** (tolérance 1e-3).
- **Monte Carlo** : |prix MC − prix BS| < 3 × erreur standard (500 000 trajectoires, seed fixée).
- **Grecques différences finies BS ≈ analytiques** (tolérance 1e-4 relative).
- Entrées invalides → exception.

## Commandes
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/apps/pricer_cli            # démo : prix + grecques des 3 méthodes

# Référence Python
cd python && uv run reference.py   # ou : pip install numpy scipy && python reference.py
```

## Règles de travail pour Claude Code
1. Lire `ROADMAP.md`, ne travailler **que sur l'étape en cours**.
2. **Avant de coder** : expliquer en français simple ce qui va être fait et pourquoi (maths + choix C++).
3. Écrire les tests **en même temps** que le code ; tout doit compiler sans warning et tous les tests passer.
4. Après chaque étape : résumé court (ce qui a été fait, comment le vérifier, ce que je dois savoir
   expliquer en entretien), puis proposer le message de commit. **Ne pas enchaîner l'étape suivante
   sans validation.**
5. Fichiers complets plutôt que patchs partiels quand un fichier est modifié en profondeur.
6. Ne jamais inventer de résultats dans le README : tous les chiffres viennent d'une exécution réelle.