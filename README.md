# TAGE vs L-TAGE Branch Predictor

TAGE branch predictor is developed by Seznec et al. as an extension to the PPM-like branch predictor. Just like PPM, it combines several predictors. Unlike PPM though, Tage uses geometric history lengths to index each compoenent. This use of geometric series of history lengths allows TAGE to exploit very long history lengths as well as to capture correlation on recent branch outcomes. TAGE also introduced a new and efficient update policy, a slight variation compared to the PPM-like branch predictor. 

L-TAGE branch predictor is an extension to the TAGE branch predictor. It introduces another special predictor component called the loop predictor. It's task is to identify regular loops with constant number of iterations. It provides the global prediction only when the loop has been executed 3 times successfully. 

In this project, L-TAGE branch predictor is implemented and compared its performance with the TAGE predictor using the ChampSim (Championship Simulator).

Code for the tage, l-tage, and other branch predictors can be found in the `ChampSim/branch` directory


## Characteristics of the TAGE and L-TAGE predictor components used

|               | Base     | T1, T2   | T3, T4   | T5    | T6    | T7    | T8, T9    | T10   | T11   | T12   |
|:---------------:|:----------:|:----------:|:----------:|:-------:|:-------:|:-------:|:-----------:|:-------:|:-------:|:-------:|
| history len   |          | 4, 6     | 10, 16   | 25    | 40    | 64    | 101, 160  | 254   | 403   | 640   |
| #entries      | 16K      | 1K       | 1K       | 1K    | 1K    | 1K    | 1K        | 1K    | 1K    | 1K    |
| Tag width     |          | 7        | 8        | 9     | 10    | 11    | 12        | 13    | 14    | 15    |

The implemented loop predictor has 256 entries and is 4-way associative. Each entry contains past iteration count, current iteration count, tag, confidence counter, and an age counter.
- past iteration count: **14 bits**
- current iteration count: **14 bits**
- tag: **14 bits**
- confidence counter: **2 bits**
- age counter: **8 bits**

The loop predictor in total uses 256 * 52 = **13Kbits**


## Traces

- Traces used in this project can be found at [NCSU IPC](https://research.ece.ncsu.edu/ipc/infrastructure/) and can be downloaded from here [traces](https://drive.google.com/file/d/1qs8t8-YWc7lLoYbjbH_d3lf1xdoYBznf/view?usp=sharing)


## Instructions to run

Depending on the timeline you are living, you might see a different versions of the [ChampSim](https://github.com/ChampSim/ChampSim/) repository. I've updated the project to the latest version.

You can find more information about setting the configurations inside the `ChampSim` directory (README from the official ChampSim community). In the `ChampSim/champsim_config.json` file, change the branch predictor to the one you want to test

Inside `champsim_config.json`:
```
  ...
  "ooo_cpu": [
    {
      ...
      "branch_predictor": "LTage",    // or "Tage", "hashed_perceptron", ...
      "btb": "basic_btb"
    }
  ],
  ...
  ```

After selecting the branch predictor above, run the following commands (inside the `ChampSim` directory):
```
$ make clean
$ ./config.sh champsim_config.json
$ make
```

To run the simulations:
```
$ bin/champsim --warmup-instructions [NUM-WARM] --simulation-instructions [NUM-SIM] ~/path/to/traces/tracefile.xz
```

In this project, all the traces are simulated using `NUM-WARM = 10M` and `NUM-SIM = 10M`


## References

- [L-TAGE](https://www.irisa.fr/caps/people/seznec/L-TAGE.pdf)
- [TAGE](https://jilp.org/vol8/v8paper1.pdf)
- [PPM-like tag based branch predictor](https://jilp.org/vol7/v7paper10.pdf)
- [Combining Branch Predictors](https://shiftleft.com/mirrors/www.hpl.hp.com/techreports/Compaq-DEC/WRL-TN-36.pdf)
- [L-TAGE](https://jilp.org/vol9/v9paper6.pdf)