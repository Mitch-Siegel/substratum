use clap::Parser;

#[derive(Parser)]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(clap::Subcommand)]
enum Command {
    Check,
    CheckSbc,
    BuildExample { name: String },
}

fn exec_command(
    cmd: &str,
    args: &[&str],
    inherit_stdout: bool,
    inherit_stderr: bool,
) -> std::process::Output {
    let mut command = std::process::Command::new(cmd);
    let cmd = command.args(args);

    let cmd = if inherit_stderr {
        cmd.stderr(std::process::Stdio::inherit())
    } else {
        cmd
    };

    let cmd = if inherit_stdout {
        cmd.stdout(std::process::Stdio::inherit())
    } else {
        cmd
    };

    cmd.output().unwrap()
}

fn execute_check() -> i32 {
    exec_command(
        "cargo",
        &[
            "clippy",
            "--workspace",
            "--all-targets",
            "--",
            "-D",
            "warnings",
        ],
        true,
        true,
    )
    .status
    .code()
    .unwrap_or(1)
}

fn execute_check_sbc() -> i32 {
    exec_command(
        "cargo",
        &["clippy", "--workspace", "--", "-D", "warnings"],
        true,
        true,
    )
    .status
    .code()
    .unwrap_or(1)
}

fn cargo_workspace_dir() -> std::path::PathBuf {
    std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .to_path_buf()
}

fn execute_build_example(name: &str) -> i32 {
    let mut path = cargo_workspace_dir();
    path.push("examples");

    let file_path = path.clone().with_file_name(name).with_extension(".sb");

    let output = exec_command(
        "cargo",
        &[
            "run",
            "--bin",
            "sbc",
            "--",
            "-i",
            file_path.to_str().unwrap(),
            "-crate-path",
            path.to_str().unwrap(),
            "-crate-name",
            name,
            "-bin",
            name,
        ],
        true,
        true,
    );

    output.status.code().unwrap_or(1)
}

fn main() {
    let cli = Cli::parse();

    let result = match cli.command {
        Command::Check => execute_check(),
        Command::CheckSbc => execute_check_sbc(),
        Command::BuildExample { name } => execute_build_example(&name),
    };
    std::process::exit(result);
}
