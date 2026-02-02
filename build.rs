fn main() {
    cc::Build::new()
        .cpp(true)
        .file("src/hr.cpp")
        .compile("hindmarsh-rose-v2-cpp");
}
