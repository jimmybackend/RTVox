mod common;
mod decoder;
mod encoder;

use std::env;

fn main() {
    let args = env::args().collect::<Vec<_>>();

    if args.len() != 4 {
        eprintln!("Usage:");
        eprintln!("  cargo run --release -- enc input.wav output.rtvx");
        eprintln!("  cargo run --release -- dec input.rtvx output.wav");
        std::process::exit(1);
    }

    let result = match args[1].as_str() {
        "enc" => encoder::run(&args[2], &args[3]),
        "dec" => decoder::run(&args[2], &args[3]),
        _ => Err("First argument must be 'enc' or 'dec'".to_string()),
    };

    if let Err(err) = result {
        eprintln!("Error: {}", err);
        std::process::exit(1);
    }
}
