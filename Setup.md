# SystemC Model Setup Instructions
## Step 1:  Build from source
```bash
mkdir -p ~/systemc && cd ~/systemc
curl -L -o systemc-2.3.4.tar.gz \
  https://github.com/accellera-official/systemc/archive/refs/tags/2.3.4.tar.gz
tar -xzf systemc-2.3.4.tar.gz && cd systemc-2.3.4
mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=$HOME/systemc/install
make -j$(sysctl -n hw.ncpu) && make install
```

## Step 2: Set env vars (add to ~/.zshrc)
```bash
export SYSTEMC_HOME=$HOME/systemc/install
export SYSTEMC_INCLUDE=$SYSTEMC_HOME/include
export SYSTEMC_LIBDIR=$SYSTEMC_HOME/lib
```

## reload the shell configuration
```bash
source ~/.zshrc
```

## create project directory and copy the SystemC model files into it
```bash
mkdir -p ~/timer_project && cd ~/timer_project
```


## Step 3: compile and run the SystemC model
```bash
/usr/bin/clang++ \
    -std=c++17 \
    -I"$SYSTEMC_INCLUDE" \
    timer_tb.cpp \
    -L"$SYSTEMC_LIBDIR" \
    -lsystemc \
    -o timer_sim
    ```
### Run
```bash
DYLD_LIBRARY_PATH="$SYSTEMC_LIBDIR" ./timer_sim # or just run ./timer_sim if you have set the DYLD_LIBRARY_PATH in your shell environment.
gtkwave timer_wave.vcd #(if gtkwave is installed) to view the waveform.
```