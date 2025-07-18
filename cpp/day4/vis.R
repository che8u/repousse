library(tidyverse)
library(gganimate)
library(gifski)
library(fs)
library(progress)
library(here)

GRID_SIZE      <- 64
frames_dir     <- here::here("gol_frames_texture")
OUTPUT_GIF     <- here::here("gol_animation.gif")
GIF_WIDTH      <- 512
GIF_HEIGHT     <- 512
GIF_FPS        <- 1

csv_files <- dir_ls(frames_dir, regexp = "\\.csv$")
pb <- progress_bar$new(
  format = " Reading frames [:bar] :percent eta: :eta",
  total = length(csv_files), clear = FALSE, width = 60
)

read_with_progress <- function(file) {
  pb$tick()
  read_csv(
    file,
    col_names = FALSE,
    col_types = cols(.default = "i"),
    progress  = FALSE
  )
}

message("Reading and processing frame data...")
life_data <- csv_files %>%
  map_dfr(read_with_progress, .id = "filepath") %>%
  group_by(filepath) %>%
  mutate(row = row_number()) %>%
  ungroup() %>%
  filter(row <= GRID_SIZE) %>%
  pivot_longer(
    cols      = paste0("X", seq_len(GRID_SIZE)),
    names_to  = "col",
    values_to = "state"
  ) %>%
  mutate(
    generation = as.integer(str_extract(filepath, "\\d+")),
    col        = as.integer(str_remove(col, "X")),
    state      = as.factor(state)
  ) %>%
  filter(state == 1) %>% # keeps living cells only
  select(generation, row, col) %>%
  arrange(generation, row, col)

message(paste("\nSuccessfully processed", n_distinct(life_data$generation), "generations."))

message("Building animation frames …")
cell_size <- 0.9 # <-- 0.9 avoids visual overlap
grid_col  <- "grey80"

static_plot <- ggplot(life_data, aes(x = col, y = row)) +
  geom_tile(width     = cell_size,
            height    = cell_size,
            fill      = "#6cc644",
            colour    = grid_col,
            linewidth = 0.15) + 
              scale_x_continuous(limits = c(0.5, GRID_SIZE + 0.5), expand = c(0, 0)) +
              scale_y_reverse(limits = c(GRID_SIZE + 0.5, 0.5), expand = c(0, 0)) +
	      coord_fixed(clip = "off") +
              theme_void() +
              theme(plot.background = element_rect(fill = "white", colour = NA),
                plot.title = element_text(hjust = 0.5, size  = 16, face  = "bold", colour = "black"))

anim <- static_plot +
  transition_time(generation) +
  labs(title = "Conway's Game of Life")

anim_save(
  OUTPUT_GIF,
  animation = anim,
  width     = GIF_WIDTH,
  height    = GIF_HEIGHT,
  fps       = GIF_FPS,
  renderer  = gifski_renderer()
)

message("Animation saved successfully. 👋")
