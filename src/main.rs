mod args;
mod dataset;
mod app;
mod utils;
use clap::Parser;
use args::Args;
use app::PlotterApp;
use dataset::Dataset;
use utils::{load_csv_points, get_default_color};
use std::path::PathBuf;

fn main() {
    let args = Args::parse();
    let mut options = eframe::NativeOptions::default();
    options.default_theme = eframe::Theme::Light;

    eframe::run_native(
        "CactusPlot",
        options,
        Box::new(move |_cc| {
            let mut app = PlotterApp::default();
            
            // Set legend visibility based on command line args
            if let Some(active_subplot) = app.get_active_subplot_mut() {
                active_subplot.config.show_legend = !args.no_legend;
            }

            // Load files into the active subplot
            for file in args.files {
                if let Ok(points) = load_csv_points(&PathBuf::from(&file)) {
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
