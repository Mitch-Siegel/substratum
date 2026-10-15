use std::{
    collections::{HashSet, VecDeque},
    fs, path,
};

use clap::Parser;

#[derive(Parser, Debug)]
struct Args {
    manifest_dirs: Vec<String>,
    #[arg(short, long, conflicts_with = "only_crates")]
    ignore_crates: Option<Vec<String>>,
    #[arg(short, long, conflicts_with = "ignore_crates")]
    only_crates: Option<Vec<String>>,
}

mod lint_from_root;

enum ManifestEntry {
    Bin(path::PathBuf),
    Lib(path::PathBuf),
}

fn lint_from_manifest_dir(
    manifest_dir: &path::Path,
    ignore_crates: &HashSet<String>,
    only_crates: &HashSet<String>,
) {
    let manifest_path: path::PathBuf = manifest_dir.join(String::from("Cargo.toml"));

    let manifest_content = fs::read_to_string(manifest_path.clone())
        .unwrap_or_else(|e| panic!("failed to read {}: {}", manifest_path.display(), e));

    let manifest = cargo_toml::Manifest::from_str(&manifest_content)
        .unwrap_or_else(|e| panic!("failed to parse {}: {}", manifest_path.display(), e));

    let mut manifest_entries = VecDeque::<ManifestEntry>::new();

    if let Some(lib) = manifest.lib {
        manifest_entries.push_back(ManifestEntry::Lib(
            manifest_dir.join(lib.path.expect("couldn't get path of lib target")),
        ));
    } else {
        let implicit_lib_path = manifest_dir.join(path::Path::new("src/lib.rs"));
        if implicit_lib_path.exists() {
            manifest_entries.push_back(ManifestEntry::Lib(implicit_lib_path));
        }
    }

    if manifest.bin.is_empty() {
        let implicit_bin_path = manifest_dir.join(path::Path::new("src/main.rs"));
        if implicit_bin_path.exists() {
            manifest_entries.push_back(ManifestEntry::Bin(implicit_bin_path));
        }
    } else {
        for bin in manifest.bin {
            manifest_entries.push_back(ManifestEntry::Bin(
                manifest_dir.join(bin.path.expect("couldn't get path of bin target")),
            ));
        }
    }

    while let Some(entry) = manifest_entries.pop_front() {
        match entry {
            ManifestEntry::Bin(path) | ManifestEntry::Lib(path) => {
                lint_from_root::lint_from_root(&path, ignore_crates, only_crates);
            }
        }
    }
}

fn main() {
    let args = Args::parse();

    let mut ignore_crates: HashSet<String> =
        args.ignore_crates.unwrap_or_default().into_iter().collect();

    // by default, ignore std and core
    ignore_crates.insert("std".into());
    ignore_crates.insert("core".into());

    let only_crates: HashSet<String> = args.only_crates.unwrap_or_default().into_iter().collect();

    for dir in &args.manifest_dirs {
        lint_from_manifest_dir(&std::path::PathBuf::from(dir), &ignore_crates, &only_crates);
    }
}
