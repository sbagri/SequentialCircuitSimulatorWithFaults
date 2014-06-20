This program consists of only one file named Main.cpp

Compiling the program
-----------------------------
On linux terminal type: g++ -O3 Main. cpp


To run the program
-----------------------------
1) ensure *.lev, *.vec, *.eqf file of the circuit to be simulated is in same folder where the code is
2) On linux terminal type: ./a.out <Circuit Name without extension>
example: ./a.out s27


How to interpret the results
------------------------------
After completion of running of the simulator, following files would be generated
1) <Circuit Name>_S.out
2) <Circuit Name>_S.det
3) <Circuit Name>_S.ufl

These files contain circuit outputs, detected faults, and undetected faults respectively.
These files can be compared with reference *.out, *.det and *.ufl file to verify its correctness


Possible corner case
------------------------------
1) It is expected that <Ciruit Name> won't be more than 40 characters. If its more, please update #define CKT_NAME_CHARS	40
at the beginning of the file.
2) It is expected that each line in *.eqf file won't be more than (CKT_NAME_CHARS*100) characters. If it is more please increase #define CKT_NAME_CHARS	40 at the beginning of file.



Apart from above two corner cases, memory would itself scaled up depending on size of circuit. So, error due to scaling up the circuit is not expected to happen.