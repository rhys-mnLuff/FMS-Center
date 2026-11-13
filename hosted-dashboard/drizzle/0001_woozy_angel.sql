CREATE TABLE `broadcasts` (
	`id` integer PRIMARY KEY AUTOINCREMENT NOT NULL,
	`zone` integer NOT NULL,
	`kind` text DEFAULT 'flood' NOT NULL,
	`body` text NOT NULL,
	`reading_id` integer,
	`recipient_count` integer NOT NULL,
	`created_ts` integer NOT NULL
);
--> statement-breakpoint
CREATE INDEX `broadcasts_zone_ts` ON `broadcasts` (`zone`,`created_ts`);--> statement-breakpoint
CREATE TABLE `sms_sends` (
	`id` integer PRIMARY KEY AUTOINCREMENT NOT NULL,
	`broadcast_id` integer NOT NULL,
	`msisdn` text NOT NULL,
	`provider_id` text,
	`status` text DEFAULT 'queued' NOT NULL,
	`error` text,
	`updated_ts` integer NOT NULL
);
--> statement-breakpoint
CREATE INDEX `sms_sends_broadcast` ON `sms_sends` (`broadcast_id`);--> statement-breakpoint
CREATE INDEX `sms_sends_status` ON `sms_sends` (`status`);--> statement-breakpoint
CREATE TABLE `subscribers` (
	`id` integer PRIMARY KEY AUTOINCREMENT NOT NULL,
	`zone` integer NOT NULL,
	`msisdn` text NOT NULL,
	`status` text DEFAULT 'active' NOT NULL,
	`consent_ts` integer,
	`stopped_ts` integer,
	`source` text,
	`created_ts` integer NOT NULL
);
--> statement-breakpoint
CREATE UNIQUE INDEX `subscribers_zone_msisdn` ON `subscribers` (`zone`,`msisdn`);--> statement-breakpoint
CREATE INDEX `subscribers_msisdn` ON `subscribers` (`msisdn`);--> statement-breakpoint
CREATE INDEX `subscribers_zone_status` ON `subscribers` (`zone`,`status`);