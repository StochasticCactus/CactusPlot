// Declare a submodule in main.rs
mod args;
// Declare a submodule in main.rs
mod dataset;
// Declare a submodule in main.rs
mod app;
// Declare a submodule in main.rs
mod utils;
// Declare a submodule in main.rs
mod data_editor; // Add this line

// Import external modules or crates needed in main.rs
use clap::Parser;
// Import external modules or crates needed in main.rs
use args::Args;
// Import external modules or crates needed in main.rs
use app::PlotterApp;
// Import external modules or crates needed in main.rs
use dataset::Dataset;
// Import external modules or crates needed in main.rs
use utils::{load_csv_points, get_default_color};
// Import external modules or crates needed in main.rs
use std::path::PathBuf;

/// Function: explain its purpose and key arguments
fn main() {
// Variable declaration
    let args = Args::parse();
// Variable declaration
    let mut options = eframe::NativeOptions::default();
    options.default_theme = eframe::Theme::Light;

    eframe::run_native(
        "CactusPlot",
        options,
        Box::new(move |_cc| {
// Variable declaration
            let mut app = PlotterApp::default();
            
            // Set legend visibility based on command line args
            if let Some(active_subplot) = app.get_active_subplot_mut() {
                active_subplot.config.show_legend = !args.no_legend;
            }

            // Load files into the active subplot
            for file in args.files {
                if let Ok(points) = load_csv_points(&PathBuf::from(&file)) {
// Variable declaration
                    let color = get_default_color(
                        app.get_active_subplot().map_or(0, |s| s.datasets.len()) % 8
                    );
                    
                    if let Some(subplot) = app.get_active_subplot_mut() {
                        subplot.datasets.push(Dataset {
                            name: file.clone(),
                            points,
                            color,
                        });
                        app.next_name_index += 1;
                    }
                }
            }

            Box::new(app)
        }),
    )
    .unwrap();
}