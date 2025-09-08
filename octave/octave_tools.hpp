// -*- c++ -*-

// Octave wrapper for testing DSP code that executes in a separate
// process, e.g. an emulator or a remote target process.
//
// TODO:
// - Generalize to multiple in/out parameters.
// - Handle byte swap. YAGNI?


// https://docs.octave.org/v4.4.0/Getting-Started-with-Oct_002dFiles.html

#ifndef OCTAVE_TOOLS_H
#define OCTAVE_TOOLS_H

#include <octave/oct.h>
#include <sys/wait.h>

// Templated from uc_tools/linux/assert_execvp.h

// Too much hassle to include actual uc_tools due to system defines,
// so I copied the necessary macros here.

#define ABORT exit(1)
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define ERROR(...) do {LOG(__VA_ARGS__);ABORT;}while(0)

#define ASSERT(assertion) ({ \
            if(!(assertion)) { \
                ERROR("%s: %d: ASSERT FAIL: " #assertion "\n", __FILE__, __LINE__); \
            } })

#define ASSERT_ERRNO(a) ({ \
            __typeof__(a) _a = (a); \
            if(-1 == (_a)) { \
                ERROR("ASSERT FAIL: " #a ", errno = %d, %s\n", errno, strerror(errno)); \
            } })

static inline void assert_fork_execvp(int *in_fd, int *out_fd, int *out_pid,
                                      const char **argv,
                                      void (*handle_sigchld)(int sig)) {

    /* In/out naming is confusing, so use x_to_y naming. */
    int ignore;
    const int read_end = 0;
    const int write_end = 1;
    int parent_to_child[2];
    int child_to_parent[2];
    ASSERT_ERRNO(pipe(parent_to_child));
    ASSERT_ERRNO(pipe(child_to_parent));

    int pid = fork();

    /* CHILD */
    if (!pid){
        /* replace stdio with pipes and leave stderr as-is. */
        close(0); ignore = dup(parent_to_child[read_end]);
        close(1); ignore = dup(child_to_parent[write_end]);
        (void)ignore;

        /* No longer needed */
        close(parent_to_child[read_end]);
        close(parent_to_child[write_end]);
        close(child_to_parent[read_end]);
        close(child_to_parent[write_end]);

        /* execvp requires NULL-terminated array
           FIXME: only supporting single command, no args */
        ASSERT(argv);
        ASSERT(argv[0]);
        ASSERT_ERRNO(execvp(argv[0], (char **)&argv[0]));
        /* not reached (exec success or assert error exit) */
    }

    if (handle_sigchld) {
        signal(SIGCHLD, handle_sigchld);
    }

    /* PARENT */
    *in_fd  = child_to_parent[read_end];  close(child_to_parent[write_end]);
    *out_fd = parent_to_child[write_end]; close(parent_to_child[read_end]);

    *out_pid = pid;
}

// https://docs.octave.org/v4.2.2/Character-Strings-in-Oct_002dFiles.html  <- for string arg

// Run emulated code.  To keep this uniform, send it a matrix, get
// back a matrix.  Note that this assumes host and emulated
// architectures are the same.

static inline int write_matrix(Matrix& m_in, int to_process_fd);
static inline int write_matrix(Matrix& m_in, int to_process_fd) {

  FILE *to_process_f = fdopen(to_process_fd, "w");
  if (!to_process_f) {
    octave_stdout << "can't open program stdin\n";
    return -1;
  }

  dim_vector in_dims = m_in.dims();
  uint32_t in_dim[2] = {(uint32_t)in_dims(0), (uint32_t)in_dims(1)};
  octave_stdout << "dims: " << in_dim[0] << " " << in_dim[1] << "\n";
  fwrite((void*)in_dim, sizeof(uint32_t), 2, to_process_f);
  uint32_t in_n = in_dim[0] * in_dim[1];
  if (in_n) {
    float *in_data = (float*)malloc(in_n * sizeof(float));
    for (uint32_t i=0; i<in_n; i++) {
      uint32_t row = i / in_dim[1];
      uint32_t col = i % in_dim[1];
      in_data[i] = m_in(row,col);
    }
    fwrite((void*)in_data, in_n, sizeof(float), to_process_f);
    free(in_data);
  }
  fclose(to_process_f);
  return 0;
}

static inline Matrix read_matrix(int from_process_fd);
static inline Matrix read_matrix(int from_process_fd) {
  FILE *from_process_f = fdopen(from_process_fd, "r");
  if (!from_process_f) {
    octave_stdout << "can't open program stdout\n";
    exit(1);
  }
  uint32_t dim[2];
  int rv = fread((void*)dim, sizeof(uint32_t), 2, from_process_f);
  if (rv != 2) {
    octave_stdout << "bad read size\n";
    exit(1);
  }
  uint32_t rows    = dim[0];
  uint32_t columns = dim[1];
  octave_stdout << "rows = " << rows    << "\n";
  octave_stdout << "cols = " << columns << "\n";

  uint32_t out_n = dim[0] * dim[1];
  float *out_data = (float*)malloc(sizeof(float) * out_n);
  size_t n_read = fread((void*)out_data, sizeof(float_t), out_n, from_process_f);
  if (out_n != n_read) {
    LOG("n = %d, n_read = %d\n", (int)out_n, (int)n_read);
    exit(1);
  }
  fclose(from_process_f);

  Matrix m_out = Matrix(rows, columns);
  for (uint32_t i=0; i<out_n; i++) {
    uint32_t row = i / columns;
    uint32_t col = i % columns;
    m_out(row,col) = out_data[i];
  }
  free(out_data);

  return m_out;

}

static inline Matrix run_with_matrix(const char **argv, Matrix& m_in);
static inline Matrix run_with_matrix(const char **argv, Matrix& m_in) {


  int from_process_fd, to_process_fd, pid;
  assert_fork_execvp(&from_process_fd, &to_process_fd, &pid, argv, NULL);


  /* Write the input.  The other end will always read the full input
     before writing any results back, to avoid deadlocking on buffer
     sizes. */
  write_matrix(m_in, to_process_fd);


  /* Read the output. */
  Matrix m_out = read_matrix(from_process_fd);


  int status;
  waitpid(pid, &status, 0);

  return m_out;

}

static inline Matrix shell_with_matrix(const char *arg, Matrix& m_in) {
  const char *argv[] = {"/bin/sh", "-c", arg, NULL};
  return run_with_matrix(argv, m_in);
}


// Old approach: hardcoded to elf binary name and string selector argument.
#define DEFUN_TEST_ARMV7(name) \
DEFUN_DLD (name, args, nargout, "Run emulated algorithm '" #name "'") { \
  octave_value_list retval (1); \
  int nargin = args.length(); \
  if (nargin != 1) { print_usage(); } \
  else { \
    Matrix m_in = args(0).array_value(); \
    const char *argv[] = {"../armv7-nix/test_armv7.elf", #name, NULL}; \
    Matrix m_out = run_with_matrix(argv, m_in); \
    retval(0) = octave_value(m_out); \
  } \
  return retval; \
}



#endif

