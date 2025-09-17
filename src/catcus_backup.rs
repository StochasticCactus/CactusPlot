use eframe::{egui, App, Frame};
use egui_plot::{Legend, Line, Plot, PlotPoints};
use std::path::PathBuf;

struct Dataset {
    name: String,
    points: Vec<[f64; 2]>,
}

struct PlotterApp {
    datasets: Vec<Dataset>,
    show_grid: bool,
    show_legend: bool,
    next_name_index: usize,
    error_message: Option<String>,
}

impl Default for PlotterApp {
    fn default() -> Self {
        // default with a sample sine and cosine
        let mut datasets = Vec::new();
        let n = 200usize;
        let mut sin_points = Vec::with_capacity(n);
        let mut cos_points = Vec::with_capacity(n);
        for i in 0..n {
            let x = i as f64 / (n as f64) * 10.0 - 5.0;
            sin_points.push([x, (x).sin()]);
            cos_points.push([x, (x).cos()]);
        }
        datasets.push(Dataset {
            name: "sin(x)".to_owned(),
            points: sin_points,
        });
        datasets.push(Dataset {
            name: "cos(x)".to_owned(),
            points: cos_points,
        });

        Self {
            datasets,
            show_grid: false,
            show_legend: true,
            next_name_index: 1,
            error_message: None,
        }
    }
}

impl App for PlotterApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut Frame) {
        egui::TopBottomPanel::top("top_panel").show(ctx, |ui| {
            ui.horizontal(|ui| {
                if ui.button("Open CSV").clicked() {
                    if let Some(path) = pick_csv_file() {
                        match load_csv_points(&path) {
                            Ok(points) => {
                                let name = format!("data{}", self.next_name_index);
                                self.next_name_index += 1;
                                self.datasets.push(Dataset { name, points });
                                self.error_message = None;
                            }
                            Err(e) => {
                                self.error_message = Some(format!("Failed to load CSV: {}", e));
                            }
                        }
                    }
                }

                if ui.button("Clear datasets").clicked() {
                    self.datasets.clear();
                }

                ui.separator();
                ui.checkbox(&mut self.show_grid, "Grid");
                ui.checkbox(&mut self.show_legend, "Legend");

                ui.separator();
                if ui.button("Add random").clicked() {
                    // add a quick random-ish dataset
                    let mut pts = Vec::new();
                    let n = 120usize;
                    for i in 0..n {
                        let x = i as f64 / n as f64 * 10.0;
                        let y = (x * 0.5).sin() + (i as f64 % 7.0) * 0.05;
                        pts.push([x, y]);
                    }
                    let name = format!("random{}", self.next_name_index);
                    self.next_name_index += 1;
                    self.datasets.push(Dataset { name, points: pts });
                }
            });

            // Show error message if any
            if let Some(ref error) = self.error_message {
                ui.colored_label(egui::Color32::RED, error);
            }
        });

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Plot area — pan with mouse, zoom with scroll");
            ui.add_space(6.0);

            // Use horizontal layout to put controls on left, plot on right
            ui.horizontal(|ui| {
                // Left side controls
                ui.vertical(|ui| {
                    ui.set_width(200.0);
                    ui.label("Datasets:");

                    let mut remove_index: Option<usize> = None;

                    for (i, ds) in self.datasets.iter().enumerate() {
                        ui.horizontal(|ui| {
                            ui.label(format!("{}:", i + 1));
                            ui.label(&ds.name);
                            if ui.small_button("x").clicked() {
                                remove_index = Some(i);
                            }
                        });
                    }

                    if let Some(i) = remove_index {
                        self.datasets.remove(i);
                    }
                });

                ui.separator();

                // Plot on the right - takes remaining space
                ui.vertical(|ui| {
                    let mut plot = Plot::new("main_plot")
                        .height(400.0)
                        .width(ui.available_width());

                    if self.show_legend {
                        plot = plot.legend(Legend::default());
                    }
                    if self.show_grid {
                        plot = plot.show_axes([true, true]);
                    }

                    plot.show(ui, |plot_ui| {
                        for ds in &self.datasets {
                            let line = Line::new(PlotPoints::new(ds.points.clone())).name(&ds.name);
                            plot_ui.line(line);
                        }
                    });
                });
            });
        });
    }
}


// Helper: load CSV with two columns (x,y). Skips rows that fail to parse.
fn load_csv_points(path: &PathBuf) -> Result<Vec<[f64; 2]>, Box<dyn std::error::Error>> {
    let mut rdr = csv::Reader::from_path(path)?;
    let mut out = Vec::new();
    for result in rdr.records() {
        let record = result?;
        if record.len() < 2 {
            continue;
        }
        if let (Ok(x), Ok(y)) = (
            record.get(0).unwrap().trim().parse::<f64>(),
            record.get(1).unwrap().trim().parse::<f64>(),
        ) {
            out.push([x, y]);
        }
        // Skip rows that can't be parsed as numbers
    }
    Ok(out)
}

// Helper: open a file dialog (CSV) using rfd
fn pick_csv_file() -> Option<PathBuf> {
    rfd::FileDialog::new()
        .add_filter("csv", &["csv"])
        .pick_file()
}

fn main() {
    let options = eframe::NativeOptions::default();
    eframe::run_native(
        "CactusPlot",
        options,
        Box::new(|_cc| Box::new(PlotterApp::default())),
    )
    .unwrap();
}
