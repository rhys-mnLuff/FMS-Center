CREATE TABLE `readings` (
	`id` integer PRIMARY KEY AUTOINCREMENT NOT NULL,
	`node` integer NOT NULL,
	`ts` integer NOT NULL,
	`received_ts` integer NOT NULL,
	`soil` integer NOT NULL,
	`water` integer NOT NULL,
	`health` integer,
	`rain` integer,
	`batt_pct` integer,
	`batt_mv` integer,
	`batt_ma` integer,
	`risk` integer,
	`lat` real,
	`lon` real,
	`sample_id` text
);
--> statement-breakpoint
CREATE INDEX `readings_node_ts` ON `readings` (`node`,`ts`);--> statement-breakpoint
CREATE INDEX `readings_ts` ON `readings` (`ts`);--> statement-breakpoint
CREATE UNIQUE INDEX `readings_node_sample` ON `readings` (`node`,`sample_id`);