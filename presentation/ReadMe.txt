Trajanje prezentacije: približno 7 minuta.

Planirana struktura:
1. Problem i cilj projekta: konverzija MATPOWER trofaznih podataka u dTwin WLS modele.
2. Tok rada konvertora: parsiranje MATPOWER podataka, formiranje Ybus matrice i generisanje .dmodl i .vmodl fajlova.
3. Model estimacije stanja: kompleksne varijable napona, WLS jednačine, težine mjerenja i Gaussov šum na mjerenim kompleksnim injekcijama snage.
4. Napomene o implementaciji: natID GUI, dense matrice za numeričke matrice, sparse matrice za rijetke koordinatne podatke i CMake build setup.
5. Rezultati i validacija: dTwin grafovi, konvergencija, amplitude i uglovi napona, te poređenje sa IEEE 4-bus referentnim rezultatima.
6. Zaključak: automatizovan tok od MATPOWER ulaza do dTwin rezultata estimacije stanja.


Presentation duration: approximately 7 minutes.
Planned structure:
1. Project problem and goal: MATPOWER three-phase data conversion to dTwin WLS models.
2. Converter workflow: parsing MATPOWER data, forming Ybus, and generating .dmodl/.vmodl files.
3. State-estimation model: complex voltage variables, WLS equations, measurement weights, and Gaussian noise on measured complex power injections.
4. Implementation notes: natID GUI, dense matrices for numerical matrices, sparse matrices for sparse coordinate data, and CMake build setup.
5. Results and validation: dTwin graphs, convergence, voltage magnitudes/angles, and comparison with the IEEE 4-bus reference results.
6. Conclusion: automated workflow from MATPOWER input to dTwin state-estimation results.
