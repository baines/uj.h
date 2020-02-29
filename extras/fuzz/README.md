Fuzzing setup using AFL

Run the following to fuzz the lexer and parser:
- `make`
- `make run` # starts AFL
  - (let it run for ~5min to get 100% branch coverage)
- `make cov` # generate lcov coverage report
- open the `output/cov/web/index.html` in a browser

Let it run longer if you want to test more paths.

I've run it for quite a while with no hangs or crashes detected.
