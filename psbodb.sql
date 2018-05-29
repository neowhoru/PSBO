-- ------------------------------------------------------
-- Server version	10.1.23-MariaDB-9+deb9u1

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `accepted_gifts`
--

DROP TABLE IF EXISTS `accepted_gifts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `accepted_gifts` (
  `GiftID` bigint(20) unsigned DEFAULT NULL,
  `PlayerID` int(10) unsigned NOT NULL,
  `ItemID` int(10) unsigned NOT NULL,
  `send_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `message` text NOT NULL,
  `sender` tinytext NOT NULL,
  `DayCount` int(11) NOT NULL DEFAULT '0',
  `ShopID` bigint(20) NOT NULL DEFAULT '-1',
  `GiftState` tinyint(4) NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `accounts`
--

DROP TABLE IF EXISTS `accounts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `accounts` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(21) NOT NULL,
  `passwd` varchar(74) NOT NULL,
  `banned` bigint(20) unsigned NOT NULL DEFAULT '0',
  `character_name` varchar(21) NOT NULL,
  `salt` varchar(10) NOT NULL DEFAULT '',
  `email` varchar(64) DEFAULT NULL,
  `reg_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `ban_reason` text,
  `last_login` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `login_ip` varchar(64) DEFAULT NULL,
  `pw_reset_ticket` varchar(64) DEFAULT NULL,
  `pw_reset_time` bigint(20) NOT NULL DEFAULT '0',
  `pw_reset_enabled` tinyint(4) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_accounts_name` (`name`),
  UNIQUE KEY `pw_reset_ticket_UNIQUE` (`pw_reset_ticket`)
) ENGINE=InnoDB AUTO_INCREMENT=6987 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `backup_season_records`
--

DROP TABLE IF EXISTS `backup_season_records`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `backup_season_records` (
  `season_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `char_id` int(10) unsigned NOT NULL DEFAULT '0',
  `mapid` int(10) unsigned NOT NULL DEFAULT '0',
  `laptime_best` float DEFAULT NULL,
  `run_count` int(10) unsigned DEFAULT NULL,
  `laptime_avarage` float DEFAULT NULL,
  `score_best` int(11) DEFAULT NULL,
  `score_avarage` int(11) DEFAULT NULL,
  `combo_best` int(11) DEFAULT NULL,
  `combo_avarage` int(11) DEFAULT NULL,
  PRIMARY KEY (`season_id`,`char_id`,`mapid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;


--
-- Table structure for table `backup_season_skillpoints`
--

DROP TABLE IF EXISTS `backup_season_skillpoints`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `backup_season_skillpoints` (
  `season_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `char_id` int(11) unsigned NOT NULL DEFAULT '0',
  `skill_points` bigint(20) NOT NULL,
  PRIMARY KEY (`season_id`,`char_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `bulletin`
--

DROP TABLE IF EXISTS `bulletin`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `bulletin` (
  `id` int(11) unsigned NOT NULL,
  `guild_id` int(11) unsigned NOT NULL DEFAULT '0',
  `name` tinytext NOT NULL,
  `msg` text NOT NULL,
  `is_deleted` int(11) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `index1` (`guild_id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `characters`
--

DROP TABLE IF EXISTS `characters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `characters` (
  `id` int(11) unsigned NOT NULL,
  `name` varchar(21) NOT NULL,
  `level` int(11) unsigned NOT NULL DEFAULT '0',
  `experience` bigint(20) unsigned NOT NULL DEFAULT '0',
  `popularity` int(11) NOT NULL DEFAULT '0',
  `guildid` int(11) unsigned NOT NULL DEFAULT '0',
  `guild_points` int(11) unsigned NOT NULL DEFAULT '0',
  `guild_rank` int(11) unsigned NOT NULL DEFAULT '1',
  `pro` int(11) unsigned NOT NULL DEFAULT '0',
  `cash` int(11) unsigned NOT NULL DEFAULT '0',
  `avatar` tinyint(3) unsigned NOT NULL DEFAULT '2',
  `race_point` int(11) NOT NULL DEFAULT '0',
  `battle_point` int(11) NOT NULL DEFAULT '0',
  `player_rank` tinyint(4) NOT NULL DEFAULT '0',
  `race_rank` int(11) unsigned NOT NULL DEFAULT '0',
  `hair2` int(11) NOT NULL DEFAULT '16777215',
  `hair3` int(11) NOT NULL DEFAULT '0',
  `hair4` int(11) NOT NULL DEFAULT '16777215',
  `hair5` int(11) NOT NULL DEFAULT '16777215',
  `gems` blob,
  `cubes` blob,
  `equips` blob,
  `userdata` blob,
  `is_online` int(11) NOT NULL DEFAULT '0',
  `last_daily_cash` bigint(20) unsigned NOT NULL DEFAULT '0',
  `skill_points` bigint(20) NOT NULL DEFAULT '0',
  `skill_season_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `voted` timestamp NULL DEFAULT NULL,
  `spruns` int(11) unsigned NOT NULL DEFAULT '0',
  `inactive_days` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `name_UNIQUE` (`name`) USING BTREE,
  KEY `index1` (`guildid`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE*/ /*!50017 DEFINER=`root`@`localhost`*/ /*!50003 TRIGGER `characters_BEFORE_UPDATE` BEFORE UPDATE ON `characters` FOR EACH ROW begin
	declare _atime bigint;
    
	if new.is_online = 0 and old.is_online = 1 then
		select unix_timestamp() - unix_timestamp(last_login) 
		into _atime
		from accounts 
		where id = old.id;
        
		if _atime > 0 then
			update online_time
			set otime = otime + _atime
			where id = old.id;

			if row_count() = 0 then
				replace into online_time 
				values (old.id, _atime);
			end if;
		end if;
    end if;
end */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;

--
-- Table structure for table `current_season_records`
--

DROP TABLE IF EXISTS `current_season_records`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `current_season_records` (
  `season_id` bigint(20) unsigned NOT NULL DEFAULT '0',
  `char_id` int(10) unsigned NOT NULL DEFAULT '0',
  `mapid` int(10) unsigned NOT NULL DEFAULT '0',
  `laptime_best` float DEFAULT NULL,
  `run_count` int(10) unsigned DEFAULT NULL,
  `laptime_avarage` float DEFAULT NULL,
  `score_best` int(11) DEFAULT NULL,
  `score_avarage` int(11) DEFAULT NULL,
  `combo_best` int(11) DEFAULT NULL,
  `combo_avarage` int(11) DEFAULT NULL,
  PRIMARY KEY (`season_id`,`char_id`,`mapid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `deleted_fashion`
--

DROP TABLE IF EXISTS `deleted_fashion`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `deleted_fashion` (
  `id` bigint(20) DEFAULT NULL,
  `character_id` int(10) unsigned NOT NULL,
  `itemid` int(10) unsigned NOT NULL,
  `itemslot` int(11) NOT NULL,
  `expiration` bigint(20) unsigned DEFAULT NULL,
  `gem1` mediumint(9) NOT NULL DEFAULT '0',
  `gem2` mediumint(9) NOT NULL DEFAULT '0',
  `gem3` mediumint(9) NOT NULL DEFAULT '0',
  `gem4` mediumint(9) NOT NULL DEFAULT '0',
  `gem5` mediumint(9) NOT NULL DEFAULT '0',
  `gem6` mediumint(9) NOT NULL DEFAULT '0',
  `gem7` mediumint(9) NOT NULL DEFAULT '0',
  `gem8` mediumint(9) NOT NULL DEFAULT '0',
  `ShopID` bigint(20) NOT NULL DEFAULT '-1',
  `color` int(11) unsigned DEFAULT '16777215'
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exp_list`
--

DROP TABLE IF EXISTS `exp_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `exp_list` (
  `level` int(11) NOT NULL,
  `value` bigint(20) NOT NULL,
  PRIMARY KEY (`level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `exp_list`
--

LOCK TABLES `exp_list` WRITE;
/*!40000 ALTER TABLE `exp_list` DISABLE KEYS */;
INSERT INTO `exp_list` VALUES (-1,0),(0,0),(1,800),(2,2000),(3,4000),(4,6800),(5,10400),(6,16400),(7,24400),(8,34400),(9,46400),(10,62400),(11,82400),(12,107400),(13,138400),(14,176400),(15,222400),(16,278400),(17,345400),(18,425400),(19,519400),(20,629400),(21,751400),(22,893400),(23,1057400),(24,1245400),(25,1461400),(26,1707400),(27,1987400),(28,2303400),(29,2659400),(30,3057400),(31,3495400),(32,3975400),(33,4501400),(34,5077400),(35,5707400),(36,6395400),(37,7147400),(38,7965400),(39,8857400),(40,9825400),(41,10851400),(42,11901400),(43,12973400),(44,14069400),(45,15187400),(46,16327400),(47,17491400),(48,18679400),(49,19889400),(50,21129400),(51,22399400),(52,23699400),(53,25029400),(54,26389400),(55,27779400),(56,29199400),(57,30649400),(58,32129400),(59,33639400),(60,35179400),(61,36749400),(62,38349400),(63,39979400),(64,41639400),(65,43329400);
/*!40000 ALTER TABLE `exp_list` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `fashion`
--

DROP TABLE IF EXISTS `fashion`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `fashion` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `character_id` int(10) unsigned NOT NULL,
  `itemid` int(10) unsigned NOT NULL,
  `itemslot` int(11) NOT NULL,
  `expiration` bigint(20) unsigned DEFAULT NULL,
  `gem1` mediumint(9) NOT NULL DEFAULT '0',
  `gem2` mediumint(9) NOT NULL DEFAULT '0',
  `gem3` mediumint(9) NOT NULL DEFAULT '0',
  `gem4` mediumint(9) NOT NULL DEFAULT '0',
  `gem5` mediumint(9) NOT NULL DEFAULT '0',
  `gem6` mediumint(9) NOT NULL DEFAULT '0',
  `gem7` mediumint(9) NOT NULL DEFAULT '0',
  `gem8` mediumint(9) NOT NULL DEFAULT '0',
  `ShopID` bigint(20) NOT NULL DEFAULT '-1',
  `color` int(11) unsigned NOT NULL DEFAULT '16777215',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=78843 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `friends`
--

DROP TABLE IF EXISTS `friends`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `friends` (
  `id1` int(11) unsigned NOT NULL,
  `id2` int(11) unsigned NOT NULL,
  KEY `friends_index` (`id1`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gifts`
--

DROP TABLE IF EXISTS `gifts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `gifts` (
  `GiftID` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `PlayerID` int(10) unsigned NOT NULL,
  `ItemID` int(10) unsigned NOT NULL,
  `send_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `message` text NOT NULL,
  `sender` tinytext NOT NULL,
  `DayCount` int(11) NOT NULL DEFAULT '0',
  `ShopID` bigint(20) NOT NULL DEFAULT '-1',
  `GiftState` tinyint(4) NOT NULL DEFAULT '0',
  PRIMARY KEY (`GiftID`),
  KEY `PlayerID` (`PlayerID`)
) ENGINE=InnoDB AUTO_INCREMENT=16257 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gifts_archive`
--

DROP TABLE IF EXISTS `gifts_archive`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `gifts_archive` (
  `ShopID` bigint(20) NOT NULL DEFAULT '-1',
  `GiftID` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `PlayerID` int(10) unsigned NOT NULL,
  `ItemID` int(10) unsigned NOT NULL,
  `send_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `message` text NOT NULL,
  `sender` tinytext NOT NULL,
  `DayCount` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`GiftID`),
  KEY `PlayerID` (`PlayerID`)
) ENGINE=InnoDB AUTO_INCREMENT=16137 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `global_ranking`
--

DROP TABLE IF EXISTS `global_ranking`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `global_ranking` (
  `id` int(10) unsigned NOT NULL,
  `runtime` float NOT NULL DEFAULT '600',
  `rank` int(11) NOT NULL DEFAULT '-1',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `global_ranking2`
--

DROP TABLE IF EXISTS `global_ranking2`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `global_ranking2` (
  `id` int(10) unsigned DEFAULT NULL,
  `runtime` float NOT NULL DEFAULT '600'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guilds`
--

DROP TABLE IF EXISTS `guilds`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `guilds` (
  `id` int(11) unsigned NOT NULL,
  `name` tinytext NOT NULL,
  `msg` text,
  `rule` text,
  `master_name` tinytext NOT NULL,
  `focus` int(11) unsigned NOT NULL DEFAULT '0',
  `points` int(11) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `licenses`
--

DROP TABLE IF EXISTS `licenses`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `licenses` (
  `char_id` int(11) unsigned NOT NULL,
  `trick_id` int(11) unsigned NOT NULL,
  `is_enabled` int(11) unsigned NOT NULL DEFAULT '1',
  `runtime` float NOT NULL,
  `score` int(11) unsigned NOT NULL DEFAULT '0',
  `collision_count` int(11) unsigned NOT NULL DEFAULT '0',
  `unk` int(11) DEFAULT NULL,
  PRIMARY KEY (`char_id`,`trick_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `login_log`
--

DROP TABLE IF EXISTS `login_log`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `login_log` (
  `acc_id` int(10) unsigned NOT NULL,
  `ip` varchar(64) NOT NULL,
  `login_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `maps`
--

DROP TABLE IF EXISTS `maps`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `maps` (
  `mapid` smallint(6) NOT NULL,
  `mapname` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`mapid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `maps`
--

LOCK TABLES `maps` WRITE;
/*!40000 ALTER TABLE `maps` DISABLE KEYS */;
INSERT INTO `maps` VALUES (0,'Oblivion 1'),(1,'Eisen Watercourse 1'),(2,'Eisen Watercourse 2'),(3,'Oblivion 2'),(4,'Chachapoyas 1'),(5,'Santa Claus 1'),(6,'Jansen\'s Forest 1'),(7,'Smallpox 1'),(8,'Smallpox 2'),(9,'Equilibrium 1'),(10,'Chachapoyas 2'),(11,'Smallpox 3'),(12,'Jansen\'s Forest 2'),(13,'Eisen Watercourse 3'),(14,'Oblivion 3'),(15,'Chagall 1'),(16,'Chagall 2'),(17,'Smallpox 1 Mirror'),(18,'Santa Claus 1 Mirror'),(19,'Chagall 1 Mirror'),(20,'Chachapoyas 1 Mirror'),(21,'Oblivion 2 Mirror'),(22,'Smallpox 3 Mirror'),(23,'Smallpox 2 Mirror'),(24,'Chachapoyas 2 Mirror'),(25,'Chagall 2 Mirror'),(26,'Equilibrium 1 Mirror'),(27,'Jansen\'s Forest 1 Mirror'),(28,'Jansen\'s Forest 2 Mirror'),(29,'Eisen Watercourse 1 Mirror'),(30,'Eisen Watercourse 2 Mirror'),(32,'Chachapoyas 3'),(33,'Sand Madness'),(34,'Shangri-La'),(35,'Giant Ruin'),(36,'Smallpox Mountain');
/*!40000 ALTER TABLE `maps` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `messages`
--

DROP TABLE IF EXISTS `messages`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `messages` (
  `char_id` int(11) unsigned NOT NULL,
  `seq_id` int(11) unsigned NOT NULL,
  `msg_id` int(11) unsigned NOT NULL,
  `str1` text,
  `str2` text,
  `str3` text,
  `str4` text,
  `msg_time` bigint(20) DEFAULT NULL,
  PRIMARY KEY (`char_id`,`seq_id`),
  KEY `index1` (`seq_id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `online_player_count`
--

DROP TABLE IF EXISTS `online_player_count`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `online_player_count` (
  `id` int(11) NOT NULL,
  `player_count` int(11) NOT NULL,
  `update_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `online_player_count`
--

LOCK TABLES `online_player_count` WRITE;
/*!40000 ALTER TABLE `online_player_count` DISABLE KEYS */;
INSERT INTO `online_player_count` VALUES (0,0,'0000-00-00 00:00:00');
/*!40000 ALTER TABLE `online_player_count` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `online_time`
--

DROP TABLE IF EXISTS `online_time`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `online_time` (
  `id` int(10) unsigned NOT NULL,
  `otime` bigint(20) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `previous_seasonal_ranking`
--

DROP TABLE IF EXISTS `previous_seasonal_ranking`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `previous_seasonal_ranking` (
  `season_id` bigint(20) NOT NULL,
  `id` int(10) unsigned NOT NULL,
  `skill_points` bigint(20) DEFAULT NULL,
  `rank` int(11) DEFAULT NULL,
  PRIMARY KEY (`season_id`,`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `psbo_schedule`
--

DROP TABLE IF EXISTS `psbo_schedule`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `psbo_schedule` (
  `id` int(11) NOT NULL,
  `last_inactivity_check` datetime DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `psbo_schedule`
--

LOCK TABLES `psbo_schedule` WRITE;
/*!40000 ALTER TABLE `psbo_schedule` DISABLE KEYS */;
INSERT INTO `psbo_schedule` VALUES (0,'2018-05-24 00:00:01');
/*!40000 ALTER TABLE `psbo_schedule` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `records`
--

DROP TABLE IF EXISTS `records`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `records` (
  `char_id` int(11) unsigned NOT NULL,
  `mapid` int(11) unsigned NOT NULL,
  `laptime_best` float DEFAULT NULL,
  `run_count` int(11) unsigned DEFAULT NULL,
  `laptime_avarage` float DEFAULT NULL,
  `score_best` int(11) DEFAULT NULL,
  `score_avarage` int(11) DEFAULT NULL,
  `combo_best` int(11) DEFAULT NULL,
  `combo_avarage` int(11) DEFAULT NULL,
  PRIMARY KEY (`char_id`,`mapid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `reports`
--

DROP TABLE IF EXISTS `reports`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `reports` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `reporter` text,
  `target_player` text,
  `reason` text,
  `report_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `playerid` int(10) unsigned DEFAULT NULL,
  `mapid` int(11) NOT NULL DEFAULT '-1',
  `handled` int(10) unsigned DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=9251 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `run`
--

DROP TABLE IF EXISTS `run`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `run` (
  `id` bigint(20) NOT NULL,
  `datetime` datetime DEFAULT NULL,
  `mode` varchar(255) DEFAULT NULL,
  `room_nr` varchar(255) DEFAULT NULL,
  `player_count` smallint(6) DEFAULT NULL,
  `map` varchar(255) DEFAULT NULL,
  `season_id` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `room_pw` smallint(6) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `runresults`
--

DROP TABLE IF EXISTS `runresults`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `runresults` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `player_id` int(11) DEFAULT NULL,
  `position` int(11) DEFAULT NULL,
  `player_name` varchar(255) DEFAULT NULL,
  `player_time_client` float DEFAULT NULL,
  `player_time_server` float DEFAULT NULL,
  `used_time` int(11) DEFAULT NULL,
  `player_skillpoints` smallint(6) DEFAULT NULL,
  `player_skillpoints_change` smallint(6) DEFAULT NULL,
  `fk_run_id` bigint(20) DEFAULT NULL,
  `speedhack` smallint(6) DEFAULT NULL,
  `avatar` smallint(6) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=485236 DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `seasonal_ranking`
--

DROP TABLE IF EXISTS `seasonal_ranking`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `seasonal_ranking` (
  `id` int(10) unsigned NOT NULL,
  `skill_points` bigint(20) DEFAULT NULL,
  `rank` int(11) NOT NULL DEFAULT '-1',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `seasonal_ranking2`
--

DROP TABLE IF EXISTS `seasonal_ranking2`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `seasonal_ranking2` (
  `id` int(10) unsigned DEFAULT NULL,
  `skill_points` bigint(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `seasons`
--

DROP TABLE IF EXISTS `seasons`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `seasons` (
  `season_id` bigint(20) unsigned NOT NULL,
  PRIMARY KEY (`season_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `setitems`
--

DROP TABLE IF EXISTS `setitems`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `setitems` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `setID` int(11) NOT NULL,
  `title` varchar(100) NOT NULL,
  `itemID` int(11) NOT NULL,
  `typeName` varchar(100) NOT NULL,
  `cost` int(11) NOT NULL,
  `dayCount` int(11) NOT NULL DEFAULT '30',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=359 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `setitems`
--

LOCK TABLES `setitems` WRITE;
/*!40000 ALTER TABLE `setitems` DISABLE KEYS */;
INSERT INTO `setitems` VALUES (1,1,'Helmet',10077,'Helmet',250,30),(2,1,'Backpack',10078,'BackPack',250,30),(3,1,'UB',10079,'UB',600,30),(4,1,'LB',10080,'LB',600,30),(5,1,'Gloves',10081,'Gloves',500,30),(6,1,'Shoes',10082,'Shoes',600,30),(7,2,'Head',10479,'Head',250,30),(8,2,'Top',10480,'Top',600,30),(9,2,'Gloves',10481,'Cloves',500,30),(10,2,'Bottom',10482,'Bottom',600,30),(11,2,'Shoes',10483,'Shoes',600,30),(12,3,'Hood',10395,'Hood',250,30),(13,3,'Katana',10396,'Katana',250,30),(14,3,'Jacket',10397,'Jacket',600,30),(15,3,'Pants',10398,'Pants',600,30),(16,3,'Shoes',10399,'Shoes',600,30),(17,4,'Helmet',10461,'Helmet',250,30),(18,4,'Top',10462,'Top',600,30),(19,4,'Bottom',10463,'Bottom',600,30),(20,4,'Gloves',10464,'Gloves',500,30),(21,4,'Shoes',10465,'Shoes',600,30),(22,5,'Wings',10457,'Wings',250,30),(23,6,'Wings',10458,'Wings',250,30),(24,7,'Hood',10537,'Hood',250,30),(25,8,'Helmet',10083,'Helmet',250,30),(26,8,'Backpack',10084,'Backpack',250,30),(27,8,'UB',10085,'UB',600,30),(28,8,'LB',10086,'LB',600,30),(29,8,'Gloves',10087,'Gloves',500,30),(30,8,'Shoes',10088,'Shoes',600,30),(31,9,'Top',10487,'Top',600,30),(32,9,'Skirt',10488,'Skirt',600,30),(33,9,'Shoes',10489,'Shoes',600,30),(34,10,'Top',10490,'Top',600,30),(35,10,'Bottom',10491,'Bottom',600,30),(36,11,'Top',10466,'Top',600,30),(37,11,'Skirt',10467,'Skirt',600,30),(38,11,'Gloves',10468,'Gloves',500,30),(39,11,'Boots',10469,'Boots',600,30),(40,12,'Top',10406,'Top',600,30),(41,12,'Bottom',10407,'Bottom',600,30),(42,13,'Helmet',10089,'Helmet',250,30),(43,13,'Backpack',10090,'Backpack',250,30),(44,13,'UB',10091,'UB',600,30),(45,13,'LB',10092,'LB',600,30),(46,13,'Gloves',10093,'Gloves',500,30),(47,13,'Shoes',10094,'Shoes',600,30),(48,14,'Backpack',10539,'Backpack',250,30),(49,15,'Hood',10470,'Hood',250,30),(50,15,'Top',10471,'Top',600,30),(51,15,'Bottom',10472,'Bottom',600,30),(52,15,'Gloves',10473,'Gloves',500,30),(53,15,'Shoes',10474,'Shoes',600,30),(54,15,'Shuriken',10475,'Shuriken',250,30),(55,16,'Jacket',10435,'Jacket',600,30),(56,16,'Pants',10436,'Pants',600,30),(57,16,'Shoes',10437,'Shoes',600,30),(58,17,'Wings',10459,'Wings',250,30),(59,18,'Top',10484,'Top',600,30),(60,18,'Skirt',10485,'Skirt',600,30),(61,18,'Heels',10486,'Heels',600,30),(62,19,'Wings',10460,'Wings',250,30),(63,20,'Backpack',10540,'Backpack',250,30),(64,21,'Top',10476,'Top',600,30),(65,21,'Skirt',10477,'Skirt',600,30),(66,21,'Heels',10478,'Heels',600,30),(67,22,'Top',10410,'Top',600,30),(68,22,'Bottom',10411,'Bottom',600,30),(69,23,'Top',10747,'Top',600,30),(70,23,'Bottom',10748,'Bottom',600,30),(71,23,'Gloves',10749,'Gloves',500,30),(72,23,'Shoes',10750,'Shoes',600,30),(73,24,'Top',10760,'Top',600,30),(74,24,'Pants',10761,'Pants',600,30),(75,24,'Gloves',10762,'Gloves',500,30),(76,24,'Boot',10763,'Boot',600,30),(77,25,'MAX (Jack)',10724,'MAX (Jack)',600,30),(78,25,'ED (Ross)',10722,'ED (Ross)',600,30),(79,25,'Beth (Sunny)',10723,'Beth (Sunny)',600,30),(80,25,'Nikki (Rose)',10725,'Nikki (Rose)',600,30),(81,26,'MAX (Jack)',10731,'MAX (Jack)',600,30),(82,26,'ED (Ross)',10730,'ED (Ross)',600,30),(83,26,'Beth (Sunny)',10732,'Beth (Sunny)',600,30),(84,26,'Nikki (Rose)',10733,'Nikki (Rose)',600,30),(85,27,'MAX (Jack)',10737,'MAX (Jack)',600,30),(86,27,'ED (Ross)',10738,'ED (Ross)',600,30),(87,27,'Beth (Sunny)',10739,'Beth (Sunny)',600,30),(88,27,'Nikki (Rose)',10736,'Nikki (Rose)',600,30),(89,28,'MAX (Jack)',10742,'MAX (Jack)',600,30),(90,28,'ED (Ross)',10741,'ED (Ross)',600,30),(91,28,'Beth (Sunny)',10740,'Beth (Sunny)',600,30),(92,28,'Nikki (Rose)',10743,'Nikki (Rose)',600,30),(93,29,'MAX (Jack)',10753,'MAX (Jack)',600,30),(94,29,'ED (Ross)',10754,'ED (Ross)',600,30),(95,29,'Beth (Sunny)',10752,'Beth (Sunny)',600,30),(96,29,'Nikki (Rose)',10751,'Nikki (Rose)',600,30),(97,30,'MAX (Jack)',10757,'MAX (Jack)',600,30),(98,30,'ED (Ross)',10756,'ED (Ross)',600,30),(99,30,'Beth (Sunny)',10759,'Beth (Sunny)',600,30),(100,30,'Nikki (Rose)',10758,'Nikki (Rose)',600,30),(101,31,'Shoes',10765,'Shoes',600,30),(102,31,'Glove',10766,'Glove',500,30),(103,31,'Pants',10767,'Pants',600,30),(104,31,'Top',10768,'Top',600,30),(105,32,'Top',10769,'Top',600,30),(106,32,'Bottom',10770,'Bottom',600,30),(107,32,'Gloves',10771,'Gloves',500,30),(108,32,'Shoes',10772,'Shoes',600,30),(109,33,'Top',10777,'Top',600,30),(110,33,'Bottom',10778,'Bottom',600,30),(111,33,'Gloves',10779,'Gloves',500,30),(112,33,'Shoes',10780,'Shoes',600,30),(113,34,'Wings',10797,'Wings',250,30),(114,35,'Wings',10798,'Wings',250,30),(115,36,'Cap',10389,'Cap',250,30),(116,36,'Jacket',10390,'Jacket',600,30),(117,36,'Jeans',10388,'Jeans',600,30),(118,37,'Hoodie',10381,'Hoodie',600,30),(119,37,'Shorts',10391,'Shorts',600,30),(120,38,'Polo R Cap',10386,'Polo R Cap',250,30),(121,38,'Jacket',10387,'Jacket',600,30),(122,38,'Pants',10571,'Pants',600,30),(123,39,'Jacket',10794,'Jacket',600,30),(124,39,'Glove',10805,'Glove',500,30),(125,39,'Pants',10813,'Pants',600,30),(126,39,'Boots',10825,'Boots',600,30),(127,40,'Hoodie',10796,'Hoodie',600,30),(128,40,'Miniskirt',10817,'Miniskirt',600,30),(129,40,'Boots',10823,'Boots',600,30),(130,41,'Cap',10394,'Cap',250,30),(131,41,'Jacket',10392,'Jacket',600,30),(132,41,'Pants',10393,'Pants',600,30),(133,41,'Sneakers',10385,'Sneakers',600,30),(134,42,'Cap',10384,'Cap',250,30),(135,42,'Jacket',10382,'Jacket',600,30),(136,42,'Pants',10383,'Pants',600,30),(137,42,'Sneakers',10385,'Sneakers',600,30),(138,43,'Polo R Cap',10380,'Polo R Cap',250,30),(139,43,'Jacket',10378,'Jacket',600,30),(140,43,'Jeans',10379,'Jeans',600,30),(141,43,'Shoes',10288,'Shoes',600,30),(142,44,'Jumper',10588,'Jumper',600,30),(143,44,'Pants',10572,'Pants',600,30),(144,45,'Jumper',10712,'Jumper',600,30),(145,45,'Pants',10691,'Pants',600,30),(146,3,'Glove',10434,'Glove',500,30),(147,46,'Shirtless',10404,'Shirtless',600,30),(148,46,'Stars and Stripes Trunks',10405,'Stars and Stripes Trunks',600,30),(149,47,'Shirtless',10408,'Shirtless',600,30),(150,47,'Stars and Stripes Trunks',10409,'Stars and Stripes Trunks',600,30),(151,48,'ED (Ross)',10403,'ED (Ross)',600,30),(152,48,'Beth (Sunny)',10401,'Beth (Sunny)',600,30),(153,48,'Nikki (Rose)',10402,'Nikki (Rose)',600,30),(154,48,'MAX (Jack)',10400,'MAX (Jack)',600,30),(155,39,'GT Cap',10376,'GT Cap',250,30),(156,49,'Cowboy Hat',10828,'Cowboy Hat',250,30),(157,49,'Leisure Waistcoat',10858,'Leisure Waistcoat',600,30),(158,49,'Rodeo Look Pants',10898,'Rodeo Look Pants',600,30),(159,49,'Yellow Sneakers',10919,'Yellow Sneakers',600,30),(160,50,'Sleeveless Zipper Jacket',10854,'Sleeveless Zipper Jacket',600,30),(161,50,'MK Double Black Capris',10911,'MK Double Black Capris',600,30),(162,50,'Red Han Shoes',10925,'Red Han Shoes',600,30),(163,51,'Giant Panda Helmet',10844,'Giant Panda Helmet',250,30),(164,51,'Fleece Jacket',10872,'Fleece Jacket',600,30),(165,51,'Simple Cargo Pants',10894,'Simple Cargo Pants',600,30),(166,51,'Mountaineering Boots',10921,'Mountaineering Boots',600,30),(167,52,'Mercury Violet Jacket',10857,'Mercury Violet Jacket',600,30),(168,52,'Mercury Violet Pants',10897,'Mercury Violet Pants',600,30),(169,53,'Expedition Jacket',10873,'Expedition Jacket',600,30),(170,53,'Expedition Pants',10904,'Expedition Pants',600,30),(171,53,'Expedition Shoes',10922,'Expedition Shoes',600,30),(172,54,'Snake Gear Jacket',10859,'Snake Gear Jacket',600,30),(173,54,'Snake Gear Pants',10899,'Snake Gear Pants',600,30),(174,54,'Snake Gear Shoes',10920,'Snake Gear Shoes',600,30),(175,55,'Pink Baseball Cap',10957,'Pink Baseball Cap',250,30),(176,55,'Hummingbling Parka',10993,'Hummingbling Parka',600,30),(177,55,'Hummingbling Miniskirt',11041,'Hummingbling Miniskirt',600,30),(178,55,'Hummingbling Shoes',11061,'Hummingbling Shoes',600,30),(179,56,'Red Speed Master Jacket',11009,'Red Speed Master Jacket',600,30),(180,56,'Oceanblue Shorts',11046,'Oceanblue Shorts',600,30),(181,56,'Red Speed Master Boots',11065,'Red Speed Master Boots',600,30),(182,57,'Blue Speed Master Jacket',11010,'Blue Speed Master Jacket',600,30),(183,57,'Snow White Shorts',11047,'Snow White Shorts',600,30),(184,57,'Blue Speed Master Boots',11066,'Blue Speed Master Boots',600,30),(185,58,'Ruby Flight Headgear',10952,'Ruby Flight Headgear',250,30),(186,58,'Ruby Flight Parka',10992,'Ruby Flight Parka',600,30),(187,58,'Ruby Flight Pants',11038,'Ruby Flight Pants',600,30),(188,58,'Ruby Flight Boots',11060,'Ruby Flight Boots',600,30),(189,59,'Flower Boarder Hood Parka',10984,'Flower Boarder Hood Parka',600,30),(190,59,'Flower Boarder Pants',11027,'Flower Boarder Pants',600,30),(191,60,'Bike Suit UB',10983,'Bike Suit UB',600,30),(192,60,'Bike Suit LB',11026,'Bike Suit LB',600,30),(193,60,'Suede Boots',11057,'Suede Boots',600,30),(194,61,'Crack Padding',11121,'Crack Padding',600,30),(195,61,'Crack Pants',11155,'Crack Pants',600,30),(196,61,'Crack Boots',11182,'Crack Boots',600,30),(197,62,'XENON Jacket',11122,'XENON Jacket',600,30),(198,62,'XENON Pants',11156,'XENON Pants',600,30),(199,62,'XENON Boots',11183,'XENON Boots',600,30),(200,63,'Mobius Jacket',11124,'Mobius Jacket',600,30),(201,63,'Mobius Pants',11158,'Mobius Pants',600,30),(202,63,'Mobius Shoes',11178,'Mobius Shoes',600,30),(203,64,'Lace Layered Jacket',11123,'Lace Layered Jacket',600,30),(204,64,'Lace Layered Pants',11157,'Lace Layered Pants',600,30),(205,65,'Lucid Blue Jumper',11132,'Lucid Blue Jumper',600,30),(206,65,'Lucid Blue Pants',11165,'Lucid Blue Pants',600,30),(207,65,'Lucid Blue Shoes',11181,'Lucid Blue Shoes',600,30),(208,66,'MARQUE Steel Jacket',11143,'MARQUE Steel Jacket',600,30),(209,66,'Sand Brown Pants',11172,'Sand Brown Pants',600,30),(210,66,'Trigger Shoes',11179,'Trigger Shoes',600,30),(211,67,'Sautty Red',11211,'Sautty Red',250,30),(212,67,'Red Halter Top',11224,'Red Halter Top',600,30),(213,67,'Red Miniskirt',11247,'Red Miniskirt',600,30),(214,67,'Red Rabbit Sneakers',11257,'Red Rabbit Sneakers',600,30),(215,68,'Sautty Blue',11212,'Sautty Blue',250,30),(216,68,'Blue Halter Top',11231,'Blue Halter Top',600,30),(217,68,'Black Miniskirt',11248,'Black Miniskirt',600,30),(218,68,'Blue Rabbit Sneakers',11256,'Blue Rabbit Sneakers',600,30),(219,69,'RS Cap Black',11213,'RS Cap Black',250,30),(220,69,'Slim Top Black',11230,'Slim Top Black',600,30),(221,69,'Pleated Miniskirt',11249,'Pleated Miniskirt',600,30),(222,70,'RS Cap Yellow',11210,'RS Cap Yellow',250,30),(223,70,'RS Top Orange',11228,'RS Top Orange',600,30),(224,70,'Black Chain Jeans',11243,'Black Chain Jeans',600,30),(225,70,'GT Nikki Boots',11258,'GT Nikki Boots',600,30),(226,71,'Black and White Boots',11276,'Black and White Boots',600,30),(227,71,'Black and White Pants',11277,'Black and White Pants',600,30),(228,71,'Black and White Gloves',11278,'Black and White Gloves',500,30),(229,71,'Black and White Top',11279,'Black and White Top',600,30),(230,72,'Violet Flame Boots',11280,'Violet Flame Boots',600,30),(231,72,'Violet Flame Pants',11281,'Violet Flame Pants',600,30),(232,72,'Violet Flame Gloves',11282,'Violet Flame Gloves',500,30),(233,72,'Violet Flame Top',11283,'Violet Flame Top',600,30),(234,73,'Baseball Cap',10827,'Baseball Cap Max',250,30),(235,74,'Black Beanie',10830,'Black Beanie Max',250,30),(236,75,'Black Football Helmet',10846,'Black Football Helmet Max',250,30),(237,76,'Black PP Cap',10836,'Black PP Cap Max',250,30),(238,77,'Black Skull',10832,'Black Skull Max',250,30),(239,78,'Black Vintage Helmet',10841,'Black Vintage Helmet Max',250,30),(240,79,'Blue PP Cap',10838,'Blue PP Cap Max',250,30),(241,80,'Gray Logo Cap',10833,'Gray Logo Cap Max',250,30),(242,81,'Military Logo Cap',10834,'Military Logo Cap Max',250,30),(243,82,'Red Vintage Helmet',10842,'Red Vintage Helmet Max',250,30),(244,83,'Santa Hat',10839,'Santa Hat Max',250,30),(245,84,'Snarl Helmet',10843,'Snarl Helmet Max',250,30),(246,85,'White Beanie',10830,'White Beanie Max',250,30),(247,86,'White Football Helmet',10845,'White Football Helmet Max',250,30),(248,87,'White Logo Cap',10835,'White Logo Cap Max',250,30),(249,88,'White Skull',10831,'White Skull Max',250,30),(250,89,'Woolknit Earflap Beanie',10826,'Woolknit Earflap Beanie Max',250,30),(251,90,'Yellow PP Cap',10837,'Yellow PP Cap Max',250,30),(252,91,'Yellow Vintage Helmet',10840,'Yellow Vintag Helmet Max',250,30),(253,92,'Black Beanie',10954,'Black Beanie Beth',250,30),(254,93,'Black Football Helmet',10974,'Black Football Helmet Beth',250,30),(255,94,'Black PP Cap',10961,'Black PP Cap Beth',250,30),(256,95,'Black Skull',10956,'Black Skull Beth',250,30),(257,96,'Black Vintage Helmet',10969,'Black Vintage Helmet Beth',250,30),(258,97,'Blue Ocean Cap',10950,'Blue Ocean Cap Beth',250,30),(259,98,'Blue PP Cap',10963,'Blue PP Cap Beth',250,30),(260,99,'Gray Logo Cap',10958,'Gray Logo Cap Beth',250,30),(261,100,'Military Logo Cap',10959,'Military Logo Cap Beth',250,30),(262,101,'Red Vintage Helmet',10970,'Red Vintage Helmet Beth',250,30),(263,102,'Santa Hat',10964,'Santa Hat Beth',250,30),(264,103,'Snarl Helmet',10971,'Snarl Helmet Beth',250,30),(265,104,'White Beanie',10953,'White Beanie Beth',250,30),(266,105,'White Football Helmet',10973,'White Football Helmet Beth',250,30),(267,106,'White Logo Cap',10960,'White Logo Cap Beth',250,30),(268,107,'White Skull',10955,'White Skull Beth',250,30),(269,108,'Wing Cap Black',10966,'Wing Cap Black Beth',250,30),(270,109,'Wing Cap Blue',10967,'Wing Cap Blue Beth',250,30),(271,110,'Wing Cap Yellow',10965,'Wing Cap Yellow Beth',250,30),(272,111,'Woolknit Earflap Beanie',10951,'Woolknit Earflap Beanie Beth',250,30),(273,112,'Yellow PP Cap',10962,'Yellow PP Cap Beth',250,30),(274,113,'Yellow Vintage Helmet',10968,'Yellow Vintage Helmet Beth',250,30),(275,114,'Baseball Cap',11091,'Baseball Cap Ross',250,30),(276,115,'Black Baseball Cap',11093,'Black Baseball Cap Ross',250,30),(277,116,'Black Beanie',11103,'Black Beanie Ross',250,30),(278,117,'Black Football Helmet',11108,'Black Football Helmet Ross',250,30),(279,118,'Black PP Cap',11099,'Black PP Cap Ross',250,30),(280,119,'Black Skull',11095,'Black Skull Ross',250,30),(281,120,'Black Vintage Helmet',11110,'Black Vintage Helmet Ross',250,30),(282,121,'Blue Baseball Cap',11092,'Blue Baseball Cap Ross',250,30),(283,122,'Blue PP Cap',11101,'Blue PP Cap Ross',250,30),(284,123,'Giant Panda Helmet',11106,'Giant Panda Helmet Ross',250,30),(285,124,'Gray Logo Cap',11096,'Gray Logo Cap Ross',250,30),(286,125,'Military Logo Cap',11097,'Military Logo Cap Ross',250,30),(287,126,'Red Vintage Helmet',11111,'Red Vintage Helmet Ross',250,30),(288,127,'Santa Hat',11104,'Santa Hat Ross',250,30),(289,128,'Snarl Helmet',11105,'Snarl Helmet Ross',250,30),(290,129,'White Beanie',11102,'White Beanie Ross',250,30),(291,130,'White Football Helmet',11107,'White Football Helmet Ross',250,30),(292,131,'White Logo Cap',11098,'White Logo Cap Ross',250,30),(293,132,'White Skull',11094,'White Skull Ross',250,30),(294,133,'Yellow PP Cap',11100,'Yellow PP Cap Ross',250,30),(295,134,'Yellow Vintage Helmet',11109,'Yellow Vintage Helmet Ross',250,30),(296,135,'Giant Panda Helmet',10972,'Giant Panda Helmet Beth',250,30),(297,136,'Panda Power Top',11287,'Panda Power Top',600,30),(298,136,'Panda Power Skirt',11288,'Panda Power Skirt',600,30),(299,136,'Panda Power Gloves',11289,'Panda Power Gloves',500,30),(300,137,'Cyber Outfit Top',11295,'Cyber Outfit Top',600,30),(301,137,'Cyber Outfit Pants',11296,'Cyber Outfit Pants',600,30),(302,137,'Cyber Outfit Gloves',11297,'Cyber Outfit Gloves',500,30),(303,137,'Cyber Outfit Shoes',11298,'Cyber Outfit Shoes',500,30),(304,137,'Cyber Outfit Deck',11299,'Cyber Outfit Deck',600,30),(305,138,'Cyber Demon Top',11300,'Cyber Demon Top',600,30),(306,138,'Cyber Demon Skirt',11301,'Cyber Demon Skirt',600,30),(307,138,'Cyber Demon Gloves',11302,'Cyber Demon Gloves',500,30),(308,138,'Cyber Demon Shoes',11303,'Cyber Demon Shoes',500,30),(309,138,'Cyber Demon Deck',11304,'Cyber Demon Deck',600,30),(310,139,'Neon Cat Top',11305,'Neon Cat Top',600,30),(311,139,'Neon Cat Skirt',11306,'Neon Cat Skirt',600,30),(312,139,'Neon Cat Gloves',11307,'Neon Cat Gloves',500,30),(313,139,'Neon Cat Shoes',11308,'Neon Cat Shoes',500,30),(314,139,'Neon Cat Deck',11309,'Neon Cat Deck',600,30),(315,140,'MAX (Jack)',11310,'MAX (Jack)',600,30),(316,140,'ED (Ross)',11312,'ED (Ross)',600,30),(317,140,'Beth (Sunny)',11311,'Beth (Sunny)',600,30),(318,140,'Nikki (Rose)',11313,'Nikki (Rose)',600,30),(319,141,'MAX (Jack)',11314,'MAX (Jack)',600,30),(320,141,'ED (Ross)',11316,'ED (Ross)',600,30),(321,141,'Beth (Sunny)',11315,'Beth (Sunny)',600,30),(322,141,'Nikki (Rose)',11317,'Nikki (Rose)',600,30),(323,142,'MAX (Jack)',11318,'MAX (Jack)',600,30),(324,142,'ED (Ross)',11320,'ED (Ross)',600,30),(325,142,'Beth (Sunny)',11319,'Beth (Sunny)',600,30),(326,142,'Nikki (Rose)',11321,'Nikki (Rose)',600,30),(331,143,'Danger Zone Top',11346,'Danger Zone Top',600,30),(332,143,'Danger Zone Skirt',11347,'Danger Zone Skirt',600,30),(333,143,'Danger Zone Shoes',11348,'Danger Zone Shoes',500,30),(334,143,'Danger Zone Gloves',11349,'Danger Zone Gloves',500,30),(335,143,'Danger Zone Mask',11350,'Danger Zone Mask',250,30),(336,144,'Danger Zone Top',11351,'Danger Zone Top',600,30),(337,144,'Danger Zone Pants',11352,'Danger Zone Pants',600,30),(338,144,'Danger Zone Shoes',11353,'Danger Zone Shoes',500,30),(339,144,'Danger Zone Gloves',11354,'Danger Zone Gloves',500,30),(340,144,'Danger Zone Mask',11355,'Danger Zone Mask',250,30),(341,145,'Danger Zone Top',11356,'Danger Zone Top',600,30),(342,145,'Danger Zone Pants',11357,'Danger Zone Pants',600,30),(343,145,'Danger Zone Shoes',11358,'Danger Zone Shoes',500,30),(344,145,'Danger Zone Gloves',11359,'Danger Zone Gloves',500,30),(345,145,'Danger Zone Helmet',11360,'Danger Zone Helmet',250,30),(346,146,'Danger Zone Top',11361,'Danger Zone Top',600,30),(347,146,'Danger Zone Skirt',11362,'Danger Zone Skirt',600,30),(348,146,'Danger Zone Shoes',11363,'Danger Zone Shoes',500,30),(349,146,'Danger Zone Gloves',11364,'Danger Zone Gloves',500,30),(350,147,'Tree Hugger Top',11290,'Tree Hugger Top',600,30),(351,147,'Tree Hugger Skirt',11291,'Tree Hugger Skirt',600,30),(352,147,'Tree Hugger Gloves',11292,'Tree Hugger Gloves',500,30),(353,147,'Tree Hugger Shoes',11293,'Tree Hugger Shoes',500,30),(354,147,'Tree Hugger Deck',11294,'Tree Hugger Deck',600,30),(355,148,'Beth (Sunny)',11365,'Beth (Sunny)',600,30),(356,148,'ED (Ross)',11366,'ED (Ross)',600,30),(357,148,'MAX (Jack)',11367,'MAX (Jack)',600,30),(358,148,'Nikki (Rose)',11368,'Nikki (Rose)',600,30);
/*!40000 ALTER TABLE `setitems` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sets`
--

DROP TABLE IF EXISTS `sets`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sets` (
  `id` int(21) NOT NULL AUTO_INCREMENT,
  `title` varchar(100) NOT NULL,
  `imgUrl` varchar(300) NOT NULL,
  `secondimgUrl` varchar(300) DEFAULT NULL,
  `char` varchar(5) DEFAULT NULL,
  `level` int(2) DEFAULT NULL,
  `creatorid` int(5) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=149 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sets`
--

LOCK TABLES `sets` WRITE;
/*!40000 ALTER TABLE `sets` DISABLE KEYS */;
INSERT INTO `sets` VALUES (1,'Astronaut Suit','astronautmax.png',NULL,'Max',45,NULL),(2,'Pink Gorilla','gorilla.png',NULL,'Max',25,NULL),(3,'Imperial Assassin','imperial.png',NULL,'Max',30,NULL),(4,'Red Samurai','Samurai.png',NULL,'Max',25,NULL),(5,'Angel Wings','angelfluegel.png',NULL,'Max',NULL,NULL),(6,'Demon Wings','demonfluegel.png',NULL,'Max',NULL,NULL),(7,'Apache Hood','apache.png',NULL,'Max',NULL,NULL),(8,'Astronaut Suit','astronautbeth.png',NULL,'Beth',45,NULL),(9,'Blue Maid','bluemaid.png',NULL,'Beth',15,NULL),(10,'Pink Bikini','pinkbikini.png',NULL,'Beth',10,NULL),(11,'Red Kunoichi','redjutochi.png',NULL,'Beth',20,NULL),(12,'Stars and Stripes','starsandst.png',NULL,'Beth',10,NULL),(13,'Astronaut Suit','astronautross.png',NULL,'Ross',45,NULL),(14,'Shark Backpack','shark.png',NULL,'Ross',NULL,NULL),(15,'Snow Ninja','snowninja.png',NULL,'Ross',30,NULL),(16,'Tuxedo','toxedo.png',NULL,'Ross',15,NULL),(17,'Angel Wings','angel_nikki.png',NULL,'Nikki',NULL,NULL),(18,'Blue Kimono','blue_nn.png',NULL,'Nikki',15,NULL),(19,'Demon Wings','demon_nikki.png',NULL,'Nikki',NULL,NULL),(20,'Marlin Backpack','merlin.png',NULL,'Nikki',NULL,NULL),(21,'Pink Corset','pink_corset_n.png',NULL,'Nikki',15,NULL),(22,'Stars and Stripes','stars_st_nikki.png',NULL,'Nikki',10,NULL),(23,'Skull Set','skull_sunny.png',NULL,'Beth',20,NULL),(24,'Blood Moon Set','blood_moon.png',NULL,'Beth',20,NULL),(25,'Flaggy by beowulf','flaggy_board.png',NULL,NULL,5,511),(26,'Butterfly','butterfly_board.png',NULL,NULL,5,NULL),(27,'Falling Sky','falling_sky_board.png',NULL,NULL,5,NULL),(28,'Penumbra','penumbra_board.png',NULL,NULL,5,NULL),(29,'Color Star','color_star_board.png',NULL,NULL,5,NULL),(30,'Pink Intimidation','pink_intimidation.png',NULL,NULL,5,NULL),(31,'Black Yang','black_yang_ed.png',NULL,'Ross',20,NULL),(32,'Skull Set','skull_ed.png',NULL,'Ross',20,NULL),(33,'Skull Set','jack_skull.png',NULL,'Max',20,NULL),(34,'Cyberwings White','rose_wings_white.png',NULL,'Nikki',NULL,NULL),(35,'Cyberwings Black','rose_wings_black.png',NULL,'Nikki',NULL,NULL),(36,'Campus Boy','campusboy.png',NULL,'Max',15,NULL),(37,'Campus 101','campus101.png',NULL,'Max',20,NULL),(38,'MK Gunmetal','mkgunmetal.png',NULL,'Max',15,NULL),(39,'GT','gt_nikki.png',NULL,'Nikki',20,NULL),(40,'White Sensation','white_sensation.png',NULL,'Nikki',15,NULL),(41,'White Renaissance','white_renaissance.png',NULL,'Beth',20,NULL),(42,'Red Renaissance','red_renaissance.png',NULL,'Beth',20,NULL),(43,'Black Angel','black_angel.png',NULL,'Ross',20,NULL),(44,'Interlock','interlock_max.png',NULL,'Max',10,NULL),(45,'Interlock','interlock_ross.png',NULL,'Ross',10,NULL),(46,'Stars and Stripes','starsandstripes_max.png',NULL,'Max',10,NULL),(47,'Stars and Stripes','starsandstripes_ross.png',NULL,'Ross',10,NULL),(48,'Dream Board','dreamboard.png',NULL,NULL,5,NULL),(49,'Cowboy','cowboy.png',NULL,'Max',20,NULL),(50,'Sleeveless Red','sleeveless_red.png',NULL,'Max',15,NULL),(51,'Panda','panda_max.png',NULL,'Max',20,NULL),(52,'Mercury Violet','mercuryviolet.png',NULL,'Max',10,NULL),(53,'Expedition','expedition.png',NULL,'Max',15,NULL),(54,'Snake Gear','snake_gear.png',NULL,'Max',15,NULL),(55,'Hummingbling','hummingbling.png',NULL,'Beth',20,NULL),(56,'Red Speed Master','redspeedmaster.png',NULL,'Beth',15,NULL),(57,'Blue Speed Master','bluespeedmaster.png',NULL,'Beth',15,NULL),(58,'Ruby Flight','rubyflight.png',NULL,'Beth',20,NULL),(59,'Flower Boarder','flowerboarder.png',NULL,'Beth',10,NULL),(60,'Bike Suit','bikesuit.png',NULL,'Beth',15,NULL),(61,'Crack','crack.png',NULL,'Ross',15,NULL),(62,'XENON','xenon.png',NULL,'Ross',15,NULL),(63,'Mobius','mobius.png',NULL,'Ross',15,NULL),(64,'Lace Layered','lacelayered.png',NULL,'Ross',10,NULL),(65,'Lucid Blue','lucidblue.png',NULL,'Ross',15,NULL),(66,'MARQUE Steel','marque.png',NULL,'Ross',15,NULL),(67,'Sautty Red','sauttyred.png',NULL,'Nikki',20,NULL),(68,'Sautty Blue','sauttyblue.png',NULL,'Nikki',20,NULL),(69,'Black Pleated','blackpleated.png',NULL,'Nikki',15,NULL),(70,'Orange Chain','orangechain.png',NULL,'Nikki',20,NULL),(71,'Black and White','blackandwhitemax.png',NULL,'Max',20,NULL),(72,'Violet Flame','violetflamenikki.png',NULL,'Nikki',20,NULL),(73,'Baseball Cap','baseballcapmax.png',NULL,'Max',NULL,NULL),(74,'Black Beanie','blackbeaniemax.png',NULL,'Max',NULL,NULL),(75,'Black Football Helmet','blackfootballmax.png',NULL,'Max',NULL,NULL),(76,'Black PP Cap','blackppmax.png',NULL,'Max',NULL,NULL),(77,'Black Skull','blackskullmax.png',NULL,'Max',NULL,NULL),(78,'Black Vintage Hemlet','blackvintagemax.png',NULL,'Max',NULL,NULL),(79,'Blue PP Cap','blueppmax.png',NULL,'Max',NULL,NULL),(80,'Gray Logo Cap','graylogocapmax.png',NULL,'Max',NULL,NULL),(81,'Military Logo Cap','militarylogocapmax.png',NULL,'Max',NULL,NULL),(82,'Red Vintage Helmet','redvintagemax.png',NULL,'Max',NULL,NULL),(83,'Santa Hat','santahatmax.png',NULL,'Max',NULL,NULL),(84,'Snarl Helmet','snarlmax.png',NULL,'Max',NULL,NULL),(85,'White Beanie','whitebeaniemax.png',NULL,'Max',NULL,NULL),(86,'White Football Helmet','whitefootballmax.png',NULL,'Max',NULL,NULL),(87,'White Logo Cap','whitelogocapmax.png',NULL,'Max',NULL,NULL),(88,'White Skull','whiteskullmax.png',NULL,'Max',NULL,NULL),(89,'Woolknit Earflap Beanie','woolknitearflapbeaniemax.png',NULL,'Max',NULL,NULL),(90,'Yellow PP Cap','yellowppmax.png',NULL,'Max',NULL,NULL),(91,'Yellow Vintage Helmet','yellowvintagemax.png',NULL,'Max',NULL,NULL),(92,'Black Beanie','blackbeaniebeth.png',NULL,'Beth',NULL,NULL),(93,'Black Football Helmet','blackfootballbeth.png',NULL,'Beth',NULL,NULL),(94,'Black PP Cap','blackppbeth.png',NULL,'Beth',NULL,NULL),(95,'Black Skull','blackskullbeth.png',NULL,'Beth',NULL,NULL),(96,'Black Vintage Helmet','blackvintagebeth.png',NULL,'Beth',NULL,NULL),(97,'Blue Ocean Cap','blueoceancapbeth.png',NULL,'Beth',NULL,NULL),(98,'Blue PP Cap','blueppbeth.png',NULL,'Beth',NULL,NULL),(99,'Gray Logo Cap','graylogocapbeth.png',NULL,'Beth',NULL,NULL),(100,'Military Logo Cap','militarylogocapbeth.png',NULL,'Beth',NULL,NULL),(101,'Red Vintage Helmet','redvintagebeth.png',NULL,'Beth',NULL,NULL),(102,'Santa Hat','santahatbeth.png',NULL,'Beth',NULL,NULL),(103,'Snarl Helmet','snarlbeth.png',NULL,'Beth',NULL,NULL),(104,'White Beanie','whitebeaniebeth.png',NULL,'Beth',NULL,NULL),(105,'White Football Helmet','whitefootballbeth.png',NULL,'Beth',NULL,NULL),(106,'White Logo Cap','whitelogocapbeth.png',NULL,'Beth',NULL,NULL),(107,'White Skull','whiteskullbeth.png',NULL,'Beth',NULL,NULL),(108,'Wing Cap Black','wingcapblackbeth.png',NULL,'Beth',NULL,NULL),(109,'Wing Cap Blue','wingcapbluebeth.png',NULL,'Beth',NULL,NULL),(110,'Wing Cap Yellow','wingcapyellowbeth.png',NULL,'Beth',NULL,NULL),(111,'Woolknit Earflap Beanie','woolknitearflapbeaniebeth.png',NULL,'Beth',NULL,NULL),(112,'Yellow PP Cap','yellowppbeth.png',NULL,'Beth',NULL,NULL),(113,'Yellow Vintage Helmet','yellowvintagebeth.png',NULL,'Beth',NULL,NULL),(114,'Baseball Cap','baseballcapross.png',NULL,'Ross',NULL,NULL),(115,'Black Baseball Cap','blackbaseballcapross.png',NULL,'Ross',NULL,NULL),(116,'Black Beanie','blackbeanieross.png',NULL,'Ross',NULL,NULL),(117,'Black Football Helmet','blackfootballross.png',NULL,'Ross',NULL,NULL),(118,'Black PP Cap','blackppross.png',NULL,'Ross',NULL,NULL),(119,'Black Skull','blackskullross.png',NULL,'Ross',NULL,NULL),(120,'Black Vintage Helmet','blackvintageross.png',NULL,'Ross',NULL,NULL),(121,'Blue Baseball Cap','bluebaseballcapross.png',NULL,'Ross',NULL,NULL),(122,'Blue PP Cap','blueppross.png',NULL,'Ross',NULL,NULL),(123,'Giant Panda Helmet','giantpandaross.png',NULL,'Ross',NULL,NULL),(124,'Gray Logo Cap','graylogocapross.png',NULL,'Ross',NULL,NULL),(125,'Military Logo Cap','militarylogocapross.png',NULL,'Ross',NULL,NULL),(126,'Red Vintage Helmet','redvintageross.png',NULL,'Ross',NULL,NULL),(127,'Santa Hat','santahatross.png',NULL,'Ross',NULL,NULL),(128,'Snarl Helmet','snarlross.png',NULL,'Ross',NULL,NULL),(129,'White Beanie','whitebeanieross.png',NULL,'Ross',NULL,NULL),(130,'White Football Helmet','whitefootballross.png',NULL,'Ross',NULL,NULL),(131,'White Logo Cap','whitelogocapross.png',NULL,'Ross',NULL,NULL),(132,'White Skull','whiteskullross.png',NULL,'Ross',NULL,NULL),(133,'Yellow PP Cap','yellowppross.png',NULL,'Ross',NULL,NULL),(134,'Yellow Vintage Helmet','yellowvintageross.png',NULL,'Ross',NULL,NULL),(135,'Giant Panda Helmet','giantpandabeth.png',NULL,'Beth',NULL,NULL),(136,'Panda Power by Monomen','pandapower_beth.png',NULL,'Beth',15,1180),(137,'Cyber Outfit by Korbars','cyberoutfit_ross_clothes.png','cyberoutfit_ross_board.png','Ross',25,2061),(138,'Cyber Demon by Jezebel','cyberdemon_beth_clothes.png','cyberdemon_beth_board.png','Beth',25,1578),(139,'Neon Cat by ShiroiYuki and Jezebel','neoncat_rose_clothes.png','neoncat_rose_board.png','Nikki',25,1616),(140,'Nihil by Namika','nihil.png',NULL,NULL,5,246),(141,'Panda Love by Muzi','pandalove.png',NULL,NULL,5,523),(142,'Target Wave by fifty2','targetwave.png',NULL,NULL,5,152),(143,'Danger Zone by evul','dangerzone_beth.png',NULL,'Beth',25,71),(144,'Danger Zone by evul','danger_ed.png',NULL,'Ross',25,71),(145,'Danger Zone by evul','danger_jack.png',NULL,'Max',25,71),(146,'Danger Zone by evul','danger_rose.png',NULL,'Nikki',25,71),(147,'Tree Hugger by evul','treehugger.png','tree_hugger_deck.png','Beth',20,71),(148,'Danger Zone Deck','danger_deck.png',NULL,NULL,25,71);
/*!40000 ALTER TABLE `sets` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sumtax`
--

DROP TABLE IF EXISTS `sumtax`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `sumtax` (
  `cash` int(11) NOT NULL,
  `pro` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sumtax`
--

LOCK TABLES `sumtax` WRITE;
/*!40000 ALTER TABLE `sumtax` DISABLE KEYS */;
INSERT INTO `sumtax` VALUES (0,0);
/*!40000 ALTER TABLE `sumtax` ENABLE KEYS */;
UNLOCK TABLES;


--
-- Dumping events for database 'psbodb'
--

--
-- Dumping routines for database 'psbodb'
--
/*!50003 DROP FUNCTION IF EXISTS `accept_gift` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `accept_gift`(_playerid int unsigned, _giftid bigint unsigned, _inventory_slot int unsigned) RETURNS bigint(20)
BEGIN

declare _itemid int default 0;
declare _daycount int default 0;
declare _shopid int default 0;
declare _expiration bigint unsigned default 0;
declare _insert_id bigint default -1;
declare _shopid_count int default 0;


SELECT ItemID, DayCount, ShopID
INTO _itemid, _daycount, _shopid
FROM gifts
WHERE GiftID = _giftid AND PlayerID = _playerid AND GiftState < 2;

if _itemid > 10000 then
	UPDATE gifts
    SET GiftState = 1
	WHERE GiftID = _giftid AND PlayerID = _playerid;
    
    if _shopid != -1 THEN
		SELECT count(*)
		INTO _shopid_count
		FROM fashion
		WHERE character_id = _playerid AND ShopID = _shopid AND itemid = _itemid;
    END IF;
    
    if _shopid_count = 0 THEN
    
		IF _daycount > 0 THEN
			SET _expiration = UNIX_TIMESTAMP(NOW()) + _daycount * (60 * 60 * 24);
		END IF;
    
		INSERT INTO fashion
		SET character_id = _playerid, itemid = _itemid, itemslot = _inventory_slot, expiration = _expiration;

		SET _insert_id = LAST_INSERT_ID();

	ELSE
		SELECT id
        INTO _insert_id
        FROM fashion
		WHERE character_id = _playerid AND ShopID = _shopid AND itemid = _itemid;
    END IF;

	UPDATE gifts
    SET GiftState = 2
	WHERE GiftID = _giftid AND PlayerID = _playerid;

	INSERT INTO accepted_gifts
    SELECT *
    FROM gifts
	WHERE GiftID = _giftid AND PlayerID = _playerid;
    
    DELETE FROM gifts
	WHERE GiftID = _giftid AND PlayerID = _playerid;
    
    return _insert_id;
END IF;

RETURN -1;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `buy_item` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `buy_item`(_player_id int unsigned, _itemid int unsigned, _slot int, _expiration bigint unsigned, _price int unsigned) RETURNS bigint(20)
BEGIN
	declare r bigint default -1;
    
    UPDATE characters
    SET cash = cash - _price
    WHERE id = _player_id AND cash > _price;
    
    if ROW_COUNT() > 0 THEN
		SELECT create_item(_player_id, _itemid, _slot, _expiration)
        INTO r;
        
        IF r < 0 then
			UPDATE characters
            SET cash = cash + _price
            WHERE id = _player_id;
        END IF;
    END IF;
    
	RETURN r;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `change_password` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `change_password`(`_accname` tinytext,`oldpass` tinytext,`newpass` tinytext) RETURNS int(11)
BEGIN
	DECLARE old_saltstr tinytext;
	DECLARE new_saltstr tinytext;
	DECLARE accname tinytext;

	SET new_saltstr = SUBSTRING(MD5(RAND()), -10);
	SET accname = LOWER(_accname);

	UPDATE accounts
	SET passwd = sha2(concat(new_saltstr, sha2(newpass, 256)), 256), salt = new_saltstr
	WHERE lower(name) = accname AND passwd = sha2(concat(salt, sha2(oldpass, 256)), 256);
    
	RETURN ROW_COUNT();
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `create_item` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `create_item`(_playerid int unsigned, _itemid int unsigned, _inventory_slot int, _expiration bigint unsigned) RETURNS bigint(20)
BEGIN
    DECLARE _c1 int unsigned default 0;
    






    
    if _c1 = 0 AND _itemid > 10000 THEN
		INSERT INTO fashion
		SET character_id = _playerid, itemid = _itemid, itemslot = _inventory_slot, expiration = _expiration;
		RETURN LAST_INSERT_ID();
    ELSE
		RETURN -1;
    END IF;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `get_global_rank` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `get_global_rank`(_playerid int unsigned) RETURNS int(10) unsigned
BEGIN
	declare r int unsigned default 0;
    select rank into r
    from global_ranking
    where id = _playerid;
	RETURN r;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `get_runtime` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `get_runtime`(_char_id int unsigned, _mapid int unsigned) RETURNS float
BEGIN
DECLARE rt FLOAT default 600;
SELECT laptime_best
INTO rt
FROM records
WHERE char_id = _char_id AND mapid = _mapid;
if rt = null then
    set rt = 600;
end if;
RETURN rt;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `get_seasonal_rank` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `get_seasonal_rank`(_playerid int unsigned) RETURNS int(11)
BEGIN
	declare r int default null;
    select rank
    into r
    from seasonal_ranking
    where id = _playerid;
    
    IF r is null then
		set r = -1;
	end if;
    
	RETURN r;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `get_sum_runtimes` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `get_sum_runtimes`(char_id int unsigned) RETURNS float
BEGIN

declare s float default 0;
declare i int default 0;

WHILE i < 36 DO
  IF i != 31 THEN
		SET s = s + get_runtime(char_id, i);
	END IF;
	SET i = i + 1;
END WHILE;

RETURN s;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `item_expired` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `item_expired`(_id bigint) RETURNS int(11)
BEGIN
	DECLARE e bigint default NOW();
    SET e = e - 1;

	SELECT expiration
    INTO e
    FROM fashion
    WHERE id = _id;
    
    if e < NOW() AND e != 0 THEN
		RETURN 1;
	ELSE
		RETURN 0;
	END IF;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `login` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `login`(`_username2` TINYTEXT, `_password` TINYTEXT) RETURNS bigint(20)
BEGIN
    DECLARE nCount INT;
    DECLARE res INT;
    DECLARE saltstr tinytext;
    DECLARE pswd tinytext;
    DECLARE _username tinytext;

    SET _username = lower(_username2);

    SELECT count(*) 
    INTO nCount
    FROM accounts
    WHERE lower(name) = _username;
    
    IF nCount != 1 THEN 
	RETURN -1;
    END IF;
    

    SELECT passwd, id
    INTO pswd, res
    FROM accounts
    WHERE lower(name) = _username;
    
    SET saltstr = substr(pswd, 65);
    SET pswd = substr(pswd, 1, 64);
    
    IF sha2(concat(saltstr, sha2(_password, 256)), 256) = pswd THEN
	return res;
    ELSE
	return -1;
    END IF;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `register` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `register`(_accname tinytext, accpass tinytext, charname tinytext, email tinytext) RETURNS int(11)
BEGIN
    DECLARE nCount INT;
    DECLARE accname tinytext;
    DECLARE saltstr tinytext;
    DECLARE _password tinytext;

    SET accname = LOWER(_accname);

    IF (SELECT accname NOT REGEXP '[^a-zA-Z0-9_\-]') < 1 THEN
	return -3;
    END IF;
    IF (SELECT charname NOT REGEXP '[^a-zA-Z0-9_\-]') < 1 THEN
	return -3;
    END IF;

    SELECT count(*) 
    INTO nCount
    FROM accounts
    WHERE lower(name) = accname OR character_name = charname;
    
    
    IF nCount > 0 THEN 
	RETURN -1;
    END IF;
    IF LENGTH(accname) < 4 OR LENGTH(charname) < 4 OR LENGTH(accpass) < 4 THEN
	RETURN -2;
    END IF;

    IF LENGTH(accname) > 20 OR LENGTH(charname) > 20 OR LENGTH(accpass) > 200 then
	RETURN -6;
    END IF;

    SET saltstr = SUBSTRING(MD5(RAND()), -10);
    SET _password = SUBSTRING(MD5(RAND()), -10);
    
    INSERT INTO accounts (name, passwd, character_name, email, salt)
    VALUES (accname, concat(sha2(concat(_password, sha2(accpass, 256)), 256), _password), charname, email, saltstr);


    IF ROW_COUNT() > 0 THEN

	INSERT INTO characters
	SET id = LAST_INSERT_ID(), name = charname;

	RETURN 1;
    END IF;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `reset_pw_1` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `reset_pw_1`(_username2 tinytext, _random_ticket tinytext, _input_email2 tinytext) RETURNS tinytext CHARSET utf8
BEGIN
    declare _pw_reset_ticket tinytext default null;
    declare _pw_reset_time bigint default 0;
    declare _id int unsigned;
    declare _email tinytext;
    declare _input_email tinytext default null;
    declare _username tinytext;
    declare _return_ticket2 tinytext;

    SET _username = LOWER(_username2);
    SET _input_email = LOWER(_input_email2);

    
    if _random_ticket = "" THEN
	return '';
    end if;
    

    SELECT id, ifnull(pw_reset_ticket, ''), pw_reset_time, ifnull(email, ''), MD5(concat(id, name))
    INTO _id, _pw_reset_ticket, _pw_reset_time, _email, _return_ticket2
    FROM accounts
    WHERE lower(name) = _username
	AND unix_timestamp(NOW()) - pw_reset_time > 3600
        AND lower(email) = _input_email2;

    if _id is null OR _id = 0 then
	return '';
    end if;

    UPDATE accounts
    SET pw_reset_ticket = _random_ticket, 
	pw_reset_time = unix_timestamp(NOW()),
        pw_reset_enabled = 1
    WHERE id = _id
	AND ifnull(pw_reset_ticket, '') = _pw_reset_ticket 
        AND pw_reset_time = _pw_reset_time
        AND ifnull(email, '') = _email;

    if ROW_COUNT() = 1 THEN
	RETURN concat(_email, '?', _return_ticket2);
    end if;
    return '';
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `reset_pw_has_ticket` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `reset_pw_has_ticket`(_ticket tinytext) RETURNS int(11)
BEGIN
    declare _count bigint;
    
    SELECT ifnull(count(*), 0)
    INTO _count
    FROM accounts
    WHERE pw_reset_ticket = _ticket;
    
    if _count = 0 THEN
	return 0;
    end if;

    return 1;
        
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `start_season` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `start_season`(current_season_id bigint unsigned) RETURNS bigint(20) unsigned
BEGIN

DECLARE _last_season_id bigint unsigned default null;

SELECT max(season_id) FROM seasons
INTO _last_season_id;

IF _last_season_id IS NOT NULL THEN
	IF _last_season_id < current_season_id THEN
		REPLACE INTO backup_season_skillpoints
		SELECT skill_season_id, skill_points, id 
		FROM characters
		WHERE skill_season_id = _last_season_id;
    
		replace into previous_seasonal_ranking
		select _last_season_id, id, skill_points, rank from seasonal_ranking;

		UPDATE characters 
		SET skill_points = 0, skill_season_id = current_season_id;
	
		REPLACE INTO backup_season_records
		SELECT * 
		FROM current_season_records;

		DELETE FROM current_season_records;

		INSERT INTO seasons values (current_season_id);
	ELSE
		return 0;
  END IF;
ELSE
	INSERT INTO seasons values (current_season_id);
END IF;

RETURN current_season_id;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP FUNCTION IF EXISTS `update_password` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` FUNCTION `update_password`(_new_pw_hash tinytext, _ticket tinytext, _ticket2 tinytext) RETURNS tinytext CHARSET utf8mb4
BEGIN
    DECLARE saltstr tinytext;
    DECLARE _password tinytext;
    DECLARE _email tinytext;
    
    SET saltstr = SUBSTRING(MD5(RAND()), -10);
    SET _password = SUBSTRING(MD5(RAND()), -10);

    select email 
    into _email
    from accounts
    where pw_reset_ticket = _ticket;

    UPDATE accounts
    SET passwd = concat(sha2(concat(_password, _new_pw_hash), 256), _password), salt = saltstr, pw_reset_enabled = 0
    WHERE pw_reset_ticket = _ticket
	AND unix_timestamp(NOW()) - pw_reset_time <= 3600
        AND pw_reset_enabled = 1
        AND md5(concat(id, name)) = _ticket2;
        
        
    RETURN concat(ROW_COUNT(), '?', _email);
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP PROCEDURE IF EXISTS `calc_skillpoints` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` PROCEDURE `calc_skillpoints`()
BEGIN
DECLARE _last_season_id BIGINT UNSIGNED DEFAULT NULL;
    
SELECT max(season_id) 
    FROM seasons 
    INTO _last_season_id;
    
UPDATE `characters` 
    SET `skill_points` = `skill_points`*(1-GREATEST(((LEAST(103, `inactive_days`) -3)/100),0)) 
    WHERE `skill_season_id` = _last_season_id;
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP PROCEDURE IF EXISTS `get_global_records` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` PROCEDURE `get_global_records`()
BEGIN

DROP TEMPORARY TABLE IF EXISTS temp_distinct_mapids;
CREATE TEMPORARY TABLE IF NOT EXISTS temp_distinct_mapids AS 
(SELECT DISTINCT mapid FROM records);

SELECT *, (SELECT min(laptime_best) FROM records r2 WHERE r2.mapid = r1.mapid) 
FROM temp_distinct_mapids r1;

END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP PROCEDURE IF EXISTS `get_seasonal_records` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` PROCEDURE `get_seasonal_records`()
BEGIN

DROP TEMPORARY TABLE IF EXISTS temp_distinct_season_mapids;
CREATE TEMPORARY TABLE IF NOT EXISTS temp_distinct_season_mapids AS 
(SELECT DISTINCT mapid FROM current_season_records);

SELECT *, (SELECT min(laptime_best) FROM current_season_records r2 WHERE r2.mapid = r1.mapid) 
FROM temp_distinct_season_mapids r1;

END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP PROCEDURE IF EXISTS `remove_gems_from_all_players` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` PROCEDURE `remove_gems_from_all_players`()
BEGIN
    DECLARE ch1 int;
    declare v_id int unsigned;
    declare v_name text;
    DECLARE curs CURSOR FOR select id, name from characters;
    DECLARE CONTINUE HANDLER FOR NOT FOUND SET ch1 = 1;
 
 
    OPEN curs;
    SET ch1 = 0;
   
    main_loop: WHILE TRUE DO
        FETCH curs into v_id, v_name;
       
        IF ch1 != 0 THEN
            LEAVE main_loop;
        END IF;
 
        CALL unequip_gems(v_name);
 
    END WHILE;
   
    CLOSE curs;
 
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP PROCEDURE IF EXISTS `unequip_gems` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` PROCEDURE `unequip_gems`(_player_name text)
BEGIN
    DECLARE ch1 int default 0;
    DECLARE player_id int unsigned;
    DECLARE gem_inv_count int unsigned default 0;
    DECLARE in_gem_data BLOB;
    DECLARE i int unsigned default 0;
    declare v_id int unsigned;
    DECLARE v_gem1 mediumint;
    DECLARE v_gem2 mediumint;
    DECLARE v_gem3 mediumint;
    DECLARE v_gem4 mediumint;
    DECLARE is_found BOOLEAN;
    DECLARE t1 int;
    DECLARE gem_id mediumint;
    declare gem_count mediumint;
    DECLARE found_slot boolean;
    DECLARE curs CURSOR FOR select id, gem1, gem2, gem3, gem4 from fashion where character_id = player_id;
    DECLARE CONTINUE HANDLER FOR NOT FOUND SET ch1 = 1;
 
 
    SELECT id, gems
    INTO player_id, in_gem_data
    FROM characters
    WHERE name = _player_name;
   
       
    SET gem_inv_count = CONV(HEX(REVERSE(SUBSTR(in_gem_data, 1, 4))), 16, 10);
   
    OPEN curs;
    SET ch1 = 0;
   
    main_loop: WHILE TRUE DO
        FETCH curs into v_id, v_gem1, v_gem2, v_gem3, v_gem4;
       
        IF ch1 != 0 THEN
            LEAVE main_loop;
        END IF;
   
               
        IF v_gem1 > 0 AND v_gem1 <= 0x7fff THEN
            SET i = 0;
            SET found_slot = false;
            WHILE i < gem_inv_count DO
                SET gem_id = CONV(HEX(REVERSE(SUBSTR(in_gem_data, 5 + i * 4, 2))), 16, 10);
                SET gem_count = CONV(HEX(REVERSE(SUBSTR(in_gem_data, 5 + i * 4 + 2, 2))), 16, 10);
                SET t1 = gem_count;
 
                IF gem_id = v_gem1 then
                    SET gem_count = LEAST(gem_count + 1, 9999);
                    SET in_gem_data = CONCAT(SUBSTR(in_gem_data, 1, 4 + i * 4 + 2), REVERSE(UNHEX(LPAD(HEX(gem_count), 4, '0'))), SUBSTR(in_gem_data, 5 + i * 4 + 4));
                                        SET found_slot = true;
                    SET i = gem_inv_count;
                END IF;
               
                SET i = i + 1;
            END WHILE;
 
            IF found_slot = false THEN
                SET in_gem_data = CONCAT(in_gem_data, REVERSE(UNHEX(LPAD(HEX(v_gem1), 4, '0'))), UNHEX('0100'));
                SET gem_inv_count = gem_inv_count + 1;
                            END IF;
           
        END IF;
 
        IF v_gem2 > 0 AND v_gem2 <= 0x7fff THEN
            SET i = 0;
            SET found_slot = false;
            WHILE i < gem_inv_count DO
 
                SET gem_id = CONV(HEX(REVERSE(SUBSTR(in_gem_data, 5 + i * 4, 2))), 16, 10);
                SET gem_count = CONV(HEX(REVERSE(SUBSTR(in_gem_data, 5 + i * 4 + 2, 2))), 16, 10);
 
                IF gem_id = v_gem2 then
                    SET gem_count = LEAST(gem_count + 1, 9999);
                    SET in_gem_data = CONCAT(SUBSTR(in_gem_data, 1, 4 + i * 4 + 2), REVERSE(UNHEX(LPAD(HEX(gem_count), 4, '0'))), SUBSTR(in_gem_data, 5 + i * 4 + 4));
                    SET found_slot = true;
                    SET i = gem_inv_count;
                END IF;
               
                SET i = i + 1;
            END WHILE;
           
            IF found_slot = false THEN
                SET in_gem_data = CONCAT(in_gem_data, REVERSE(UNHEX(LPAD(HEX(v_gem2), 4, '0'))), UNHEX('0100'));
                SET gem_inv_count = gem_inv_count + 1;
            END IF;
           
        END IF;
 
        IF v_gem3 > 0 AND v_gem3 <= 0x7fff THEN
            SET i = 0;
            SET found_slot = false;
            WHILE i < gem_inv_count DO
 
                SET gem_id = CONV(HEX(REVERSE(SUBSTR(in_gem_data, 5 + i * 4, 2))), 16, 10);
                SET gem_count = CONV(HEX(REVERSE(SUBSTR(in_gem_data, 5 + i * 4 + 2, 2))), 16, 10);
 
                IF gem_id = v_gem3 then
                    SET gem_count = LEAST(gem_count + 1, 9999);
                    SET in_gem_data = CONCAT(SUBSTR(in_gem_data, 1, 4 + i * 4 + 2), REVERSE(UNHEX(LPAD(HEX(gem_count), 4, '0'))), SUBSTR(in_gem_data, 5 + i * 4 + 4));
                    SET found_slot = true;
                    SET i = gem_inv_count;
                END IF;
               
                SET i = i + 1;
            END WHILE;
           
            IF found_slot = false THEN
                SET in_gem_data = CONCAT(in_gem_data, REVERSE(UNHEX(LPAD(HEX(v_gem3), 4, '0'))), UNHEX('0100'));
                SET gem_inv_count = gem_inv_count + 1;
            END IF;
           
        END IF;
 
        IF v_gem4 > 0 AND v_gem4 <= 0x7fff THEN
            SET i = 0;
            SET found_slot = false;
            WHILE i < gem_inv_count DO
 
                SET gem_id = CONV(HEX(REVERSE(SUBSTR(in_gem_data, 5 + i * 4, 2))), 16, 10);
                SET gem_count = CONV(HEX(REVERSE(SUBSTR(in_gem_data, 5 + i * 4 + 2, 2))), 16, 10);
 
                IF gem_id = v_gem4 then
                    SET gem_count = LEAST(gem_count + 1, 9999);
                    SET in_gem_data = CONCAT(SUBSTR(in_gem_data, 1, 4 + i * 4 + 2), REVERSE(UNHEX(LPAD(HEX(gem_count), 4, '0'))), SUBSTR(in_gem_data, 5 + i * 4 + 4));
                    SET found_slot = true;
                    SET i = gem_inv_count;
                END IF;
               
                SET i = i + 1;
            END WHILE;
           
            IF found_slot = false THEN
                SET in_gem_data = CONCAT(in_gem_data, REVERSE(UNHEX(LPAD(HEX(v_gem4), 4, '0'))), UNHEX('0100'));
                SET gem_inv_count = gem_inv_count + 1;
            END IF;
           
        END IF;
       
    END WHILE;
   
    CLOSE curs;
 
    SET in_gem_data = CONCAT(REVERSE(UNHEX(LPAD(HEX(gem_inv_count), 8, '0'))), SUBSTR(in_gem_data, 5));
 
     
    UPDATE characters
    SET gems = in_gem_data
    WHERE name = _player_name;
 
    UPDATE fashion
    SET gem1 = 0, gem2 = 0, gem3 = 0, gem4 = 0
    WHERE character_id = player_id;
 
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP PROCEDURE IF EXISTS `update_inactivity` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` PROCEDURE `update_inactivity`()
BEGIN
  UPDATE characters
  SET inactive_days = inactive_days + 1
  WHERE spruns < 5;

  UPDATE characters
  SET inactive_days = 0
  WHERE spruns >= 5;
  
  UPDATE characters
  SET spruns = 0;
  
  CALL calc_skillpoints();
END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 DROP PROCEDURE IF EXISTS `update_rankings` */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_general_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
CREATE DEFINER=`root`@`localhost` PROCEDURE `update_rankings`()
BEGIN

declare _last_inactivity_check datetime default now();
DECLARE _last_season_id bigint unsigned default null;

SELECT max(season_id) FROM seasons
INTO _last_season_id;

select @rowid:=0;
drop table if exists global_ranking;
drop table if exists global_ranking2;
create table global_ranking2(id int unsigned, runtime float not null default 600) 
	select id, get_sum_runtimes(id) as runtime
	from characters
    order by runtime asc;

create table global_ranking (id int unsigned key, runtime float not null default 600, rank int not null default -1) select *, @rowid:=@rowid+1 as rank
from global_ranking2;


select @rowid2:=0;
drop table if exists seasonal_ranking;
drop table if exists seasonal_ranking2;

create table seasonal_ranking2(id int unsigned, skill_points bigint) 
	select id, skill_points
	from characters
    where skill_season_id = _last_season_id
    order by skill_points desc;

create table seasonal_ranking (id int unsigned key, skill_points bigint, rank int not null default -1) 
	select *, @rowid2:=@rowid2+1 as rank
	from seasonal_ranking2;


SELECT last_inactivity_check
INTO _last_inactivity_check
FROM psbo_schedule
WHERE id = 0;

if _last_inactivity_check is null then
	set _last_inactivity_check = now();
end if;

while(unix_timestamp(now()) - unix_timestamp(_last_inactivity_check) > 24*60*60) DO
	  SET _last_inactivity_check = from_unixtime(unix_timestamp(_last_inactivity_check) + 24*60*60);
      update psbo_schedule 
      set last_inactivity_check = _last_inactivity_check;

      call update_inactivity();
end while;

END ;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;


