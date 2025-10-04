// Declare a submodule in main.rs
mod args;
// Declare a submodule in main.rs
mod dataset;
// Declare a submodule in main.rs
mod app;
// Declare a submodule in main.rs
mod utils;
// Declare a submodule in main.rs
mod data_editor;

// Import external modules or crates needed in main.rs
use clap::Parser;
// Import external modules or crates needed in main.rs
use args::Args;
// Import external modules or crates needed in main.rs
use app::PlotterApp;
// Import external modules or crates needed in main.rs
use dataset::Dataset;
// Import external modules or crates needed in main.rs
use utils::{load_csv_points, load_xvg_points, get_default_color};
// Import external modules or crates needed in main.rs
use std::path::PathBuf;

/// Function: explain its purpose and key arguments
fn main() {
    let args = Args::parse();
    let mut options = eframe::NativeOptions::default();
    options.default_theme = eframe::Theme::Light;

    eframe::run_native(
        "CactusPlot",
        options,
        Box::new(move |_cc| {
            let mut app = PlotterApp::default();
            
            // Set grid and legend visibility based on command line args
            if let Some(active_subplot) = app.get_active_subplot_mut() {
                active_subplot.config.show_legend = !args.no_legend;
                active_subplot.config.show_grid = args.grid;
            }

            // Load files into the active subplot
            for file in args.files {
                let path = PathBuf::from(&file);
                
                // Determine file type and load accordingly
                let load_result = match path.extension().and_then(|ext| ext.to_str()) {
                    Some("csv") => load_csv_points(&path).map(|points| (points, file.clone())),
                    Some("xvg") => load_xvg_points(&path).map(|points| (points, file.clone())),
                    _ => {
                        eprintln!("Unsupported file type: {}", file);
                        continue;
                    }
                };
                
                if let Ok((points, filename)) = load_result {
                    let color = get_default_color(
                        app.get_active_subplot().map_or(0, |s| s.datasets.len()) % 8
                    );
                    
                    if let Some(subplot) = app.get_active_subplot_mut() {
                        subplot.datasets.push(Dataset {
                            name: filename,
                            points,
                            color,
                        });
                        app.next_name_index += 1;
                    }
                } else if let Err(e) = load_result {
                    eprintln!("Failed to load {}: {}", file, e);
                }
            }

            Box::new(app)
        }),
    )
    .unwrap();
}