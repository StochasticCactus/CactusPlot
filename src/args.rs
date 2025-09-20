use clap::Parser;

#[derive(Parser)]
#[command(name = "CactusPlot")]
#[command(about = "A simple but elegant plotting application")]
pub struct Args {
    /// Input data files
    #[arg(value_name = "FILE", required = false)]
    pub files: Vec<String>,

    /// Show grid on plot
    #[arg(long, action)]
    pub grid: bool,

    /// Hide the legend
    #[arg(long, action)]
    pub no_legend: bool,
}
