name: Build NEON CITY
on: [push, workflow_dispatch]

jobs:
  build:
    runs-on: ubuntu-22.04
    timeout-minutes: 25

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install deps
        run: |
          sudo apt-get update
          sudo apt-get install -y python3 python3-pip libelf-dev libtool-bin wget git
          pip3 install pycryptodome

      - name: Download toolchain
        run: |
          wget -q https://github.com/bucanero/ps3toolchain/releases/download/ubuntu-latest-fad3b5fb/ps3dev-ubuntu-latest-2020-08-31.tar.gz
          tar xzf ps3dev-ubuntu-latest-2020-08-31.tar.gz

      - name: Build SELF
        run: make all

      - name: Show self/elf
        run: ls -la neoncity.*

      - name: Make FSELF
        run: |
          if [ -f ps3dev/bin/make_fself ]; then
            chmod +x ps3dev/bin/make_fself
            ps3dev/bin/make_fself neoncity.elf neoncity.fself
          else
            cp neoncity.self neoncity.fself
          fi

      - name: Get ps3py
        run: |
          git clone --depth 1 https://github.com/ps3dev/ps3py.git /tmp/ps3py
          ls /tmp/ps3py/src/

      - name: Create PKG structure
        run: |
          mkdir -p pkgbuild/USRDIR
          cp neoncity.fself pkgbuild/USRDIR/EBOOT.BIN
          python3 tools/make_sfo.py pkgbuild/PARAM.SFO
          ls -la pkgbuild/ pkgbuild/USRDIR/

      - name: Build PKG
        continue-on-error: true
        run: |
          cd /tmp/ps3py/src
          python3 pkg.py --contentid UP0001-NEONCITY1_00-NEONCITY00000001 \
            ${{ github.workspace }}/pkgbuild/ \
            ${{ github.workspace }}/neoncity.pkg \
            && echo "PKG_BUILD_OK" || echo "PKG_BUILD_FAILED"
          ls -la ${{ github.workspace }}/*.pkg 2>/dev/null || echo "no pkg"

      - name: Final listing
        if: always()
        run: |
          echo "=== Final files ==="
          ls -la ${{ github.workspace }}/*.self \
                 ${{ github.workspace }}/*.elf \
                 ${{ github.workspace }}/*.fself \
                 ${{ github.workspace }}/*.pkg 2>/dev/null || true
          echo "=== pkgbuild dir ==="
          ls -la pkgbuild/ 2>/dev/null || true

      - name: Upload
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: neoncity-build
          path: |
            neoncity.self
            neoncity.elf
            neoncity.fself
            neoncity.pkg
            pkgbuild/**
          if-no-files-found: warn