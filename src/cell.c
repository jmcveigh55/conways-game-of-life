#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include "cell.h"
#include "utilities.h"

/*
 * Function:	cell_init
 * ----------------------
 * Initialize a unique cell.
 *
 * width: width of the cell in pixels.
 * height: height of the cell in pixels.
 *
 * returns: pointer to the newly allocated cell.
 */
static cell_t *cell_init(size_t width, size_t height)
{
	cell_t *cell_new = malloc(sizeof(*cell_new));
	if (!cell_new) {
		perror("cell_init: Failed to malloc cell_new");
		exit(EXIT_FAILURE);
	}

	cell_new->rect.w = width;
	cell_new->rect.h = height;
	cell_new->alive = 0;

	return cell_new;
}

/*
 * Function:	body_init
 * ----------------------
 * Initialize a body of cells.
 *
 * rows: number of rows in the body.
 * cols: number of cols in the body.
 *
 * returns: pointer to the newly allocated body.
 */
body_t *body_init(size_t rows, size_t cols)
{
	size_t x, y;

	body_t *body_new = malloc(sizeof(*body_new));
	if (!body_new) {
		perror("body_init: Failed to malloc body_new");
		exit(EXIT_FAILURE);
	}

	body_new->rows = rows;
	body_new->cols = cols;

	body_new->cells = malloc(body_new->rows * body_new->cols * sizeof(*body_new->cells));
	if (!body_new->cells) {
		perror("body_init: Failed to malloc body_new->cells");
		exit(EXIT_FAILURE);
	}

	for (x = 0; x < body_new->cols; x++)
		for (y = 0; y < body_new->rows; y++)
			body_new->cells[x * body_new->cols + y] = cell_init(cell_meta.width, cell_meta.height);

	return body_new;
}

/*
 * Function:	cell_destroy
 * -------------------------
 * Destroy a cell.
 *
 * cell: pointer to the cell allocated in memory.
 */
static void cell_destroy(cell_t *cell)
{
	free(cell);
}

/*
 * Function:	body_destroy
 * -------------------------
 * Destroy a body.
 *
 * body: pointer to the body allocated in memory.
 */
void body_destory(body_t *body)
{
	size_t x, y;

	for (x = 0; x < body->cols; x++)
		for (y = 0; y < body->rows; y++)
			cell_destroy(body->cells[x * body->cols + y]);
	free(body->cells);
	free(body);
}

/*
 * Function:	draw_cell
 * ----------------------
 * Draw a cell square.
 *
 * renderer: SDL_Renderer struct used for rendering the cell square.
 * cell: cell stuct stores the location, width, and hight of the cell square.
 * x: the column of the cell.
 * y: the row of the cell.
 */
static void draw_cell(SDL_Renderer *renderer, cell_t *cell, int x, int y)
{
	cell->rect.x = cell_meta.width * x;
	cell->rect.y = cell_meta.height * y;
	cell->rect.w = cell_meta.width;
	cell->rect.h = cell_meta.height;

	if (cell->alive)
		SDL_SetRenderDrawColor(renderer, cell_meta.color_r, cell_meta.color_g, cell_meta.color_b, SDL_ALPHA_OPAQUE);
	else
		SDL_SetRenderDrawColor(renderer, bg_meta.color_r, bg_meta.color_g, bg_meta.color_b, SDL_ALPHA_OPAQUE);

	SDL_RenderFillRect(renderer, &cell->rect);

	if (cell_meta.grid_on) {
		SDL_SetRenderDrawColor(renderer, 215, 215, 215, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(renderer, &cell->rect);
	}
}

/*
 * Function:	draw_generation
 * ----------------------------
 * Draw the current generation's cells.
 *
 * renderer: SDL_Renderer struct used for rendering the cell square.
 * body: the body containing the current generation of cells.
 */
void draw_generation(SDL_Renderer *renderer, body_t *body)
{
	size_t x, y;

	for (x=0; x < body->cols; x++)
		for (y=0; y < body->rows; y++)
			draw_cell(renderer, body->cells[x * body->cols + y], x, y);
}

/*
 * Function:	random_mode
 * ------------------------
 * Default mode of cell generation, randomly assigns cells as alive in the center
 * 	1/4th of the body using a probability of being alive.
 *
 * body: pointer to the body that will store the updated cells.
 * pop: pointer to the population count used for tracking the body's progress.
 *
 * returns: pointer to body with its initial conditions set.
 */
static body_t *random_mode(body_t *body, int *pop)
{
	size_t x, y;

	for (x=(size_t)(body->cols * 0.25); x < (size_t)(body->cols * 0.75); x++) {
		for (y=(size_t)(body->rows * 0.25); y < (size_t)(body->rows * 0.75); y++) {
			body->cells[x * body->cols + y]->alive = ((rand() % 100 + 1) <= cell_meta.alive_prob);
			*pop += body->cells[x * body->cols + y]->alive;
		}
	}

	return body;
}

/*
 * Function:	pattern_mode
 * ------------------------
 * A mode of cell generation, given a pattern selected by the user, generate the cells.
 *
 * body: pointer to the body that will store the updated cells.
 * pop: pointer to the population count used for tracking the body's progress.
 *
 * returns: pointer to body with its initial conditions set.
 */
static body_t *pattern_mode(body_t *body, int *pop)
{
	FILE *pattern_fd;
	size_t len, x, y;
	char *pattern, *point;

	pattern = parse_pattern_choice();

	pattern_fd = fopen(pattern, "r");
	free(pattern);
	if (!pattern_fd) {
		perror("pattern_mode: Error opening pattern file");
		return NULL;
	}

	point = NULL;
	getline(&point, &len, pattern_fd); /* Skip header */
	while (getline(&point, &len, pattern_fd) != -1) {
		point[strlen(point) - 1] = '\0';
		x = (size_t)atoi(strtok(point, ",")) + (body->cols * 0.5);
		y = (size_t)atoi(strtok(NULL, ",")) + (body->rows * 0.5);
		body->cells[x * body->cols + y]->alive = 1;
		*pop += 1;
	}

	fclose(pattern_fd);
	free(point);

	return body;
}

/*
 * Function:	drawing_mode
 * ------------------------
 * A mode of cell generation, user selects the cells to be alive and then starts simulation.
 *
 * renderer: SDL_Renderer used for rendering the window.
 * body: pointer to the body that will store the updated cells.
 * pop: pointer to the population count used for tracking the body's progress.
 *
 * returns: pointer to body with its initial conditions set.
 */
static body_t *drawing_mode(SDL_Renderer *renderer, body_t *body, int *pop)
{
	int capturing_input, x, y, i;
	SDL_Event event;
	SDL_Color color = {0, 0, 0}; /* black */
	char text[] = "DRAWING MODE";
	int temp = cell_meta.grid_on;

	cell_meta.grid_on = 1;

	SDL_RenderClear(renderer);
	draw_generation(renderer, body);
	display_body_statistics(renderer, 0, *pop);
	display_text(renderer, text, color, 24, 25, 100, 0, 0);
	SDL_RenderPresent(renderer);

	capturing_input = 1;
	while(capturing_input) {
		/* Poll for events */
		while (SDL_PollEvent(&event))
			switch (event.type) {
				case SDL_QUIT:
					return NULL;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym) {
						case SDLK_SPACE:
							capturing_input = 0;
							break;
						case SDLK_q:
							return NULL;
					}
					break;
				case SDL_MOUSEBUTTONDOWN:
					if (event.button.button == SDL_BUTTON_LEFT) {
						SDL_GetMouseState(&x, &y);
						x /= cell_meta.width;
						y /= cell_meta.height;
						i = x * body->cols + y;

						body->cells[i]->alive = !body->cells[i]->alive;
						*pop += 2 * body->cells[i]->alive - 1;

						SDL_RenderClear(renderer);
						draw_generation(renderer, body);
						display_body_statistics(renderer, 0, *pop);
						display_text(renderer, text, color, 24, 25, 100, 0, 0);
						SDL_RenderPresent(renderer);
					}
					break;
			}
	}

	cell_meta.grid_on = temp;
	return body;
}

/*
 * Function:	initial_generation
 * -------------------------------
 * The api for the selected mode to populate the initial body.
 *
 * renderer: SDL_Renderer used for rendering the window when drawing mode is selected.
 * body: pointer to the body that will store the updated cells.
 * pop: pointer to the population count used for tracking the body's progress.
 *
 * returns: pointer to body with its initial conditions set.
 */
body_t *inital_generation(SDL_Renderer *renderer, body_t *body, int *pop)
{
	*pop = 0;

	switch (mode) {
		case 'r': return random_mode(body, pop);
		case 'p': return pattern_mode(body, pop);
		case 'd': return drawing_mode(renderer, body, pop);
	}

	return NULL;
}

/*
 * Function:	compute_generation
 * -------------------------------
 * Computes the next generation given the previous generation and the rules
 * 	defining the game of life.
 *
 * body_new: pointer to the body that will store the updated cells.
 * body_old: pointer to the body that stores the previous generation of cells.
 * pop: pointer to the population count used for tracking the body's progress.
 */
void compute_generation(body_t *body_new, body_t *body_old, int *pop)
{
	int neighbors, x, y, a, b, i;
	*pop = 0;

	for (x=0; x < body_old->cols; x++) {
		for (y=0; y < body_old->rows; y++) {
			i = x * body_old->cols + y;
			neighbors = body_old->cells[i]->alive ? -1 : 0;

			for (a=-1; a < 2; a++) {
				for (b=-1; b < 2; b++) {
					if (!(x + a < 0 || x + a > body_old->cols - 1 ||
					    y + b < 0 || y + b > body_old->rows - 1))
						neighbors += body_old->cells[(x + a) * body_old->cols + (y + b)]->alive;
				}
			}

			if (body_old->cells[i]->alive && ((neighbors < 2) || (neighbors > 3)))
				body_new->cells[i]->alive = 0;
			else if (!body_old->cells[i]->alive && (neighbors == 3))
				body_new->cells[i]->alive = 1;
			else
				body_new->cells[i]->alive = body_old->cells[i]->alive;

			*pop += body_new->cells[i]->alive;
		}
	}

}

void export_body(body_t *body, int generation, int population)
{
	FILE *export_fd;
	int x, y;
	char export_file[PATH_MAX];
	time_t t = time(NULL);
  	struct tm tm = *localtime(&t);
	char *export_rel_path = "/data/patterns/export";
	char *export_path = malloc(strlen(proj_dir) + strlen(export_rel_path) + 1);
	if (!export_path) {
		perror("export_body: Failed to malloc export_path");
		exit(EXIT_FAILURE);
	}

	strcpy(export_path, proj_dir);
	strcat(export_path, export_rel_path);

	mkdir(export_path, 0755);
	sprintf(export_file, "%s/mode%c-n%d-d%d-%d-%02d-%02d-%02d:%02d:%02d.csv",
		export_path, mode, cell_meta.rows, cell_meta.height, tm.tm_year + 1900,
		tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	export_fd = fopen(export_file, "w");
	if (!export_fd) {
		perror("export_body: Error writing export file");
		goto destroy_and_exit;
	}

	fprintf(export_fd, "x,y\n");
	for (x=0; x < body->cols; x++) {
		for (y=0; y < body->rows; y++) {
			if (body->cells[x * body->cols + y]->alive)
				fprintf(export_fd, "%d,%d\n", x, y);
		}
	}

	fprintf(stderr, "Export located at %s\n", export_file);

	fclose(export_fd);
destroy_and_exit:
	free(export_path);
}
