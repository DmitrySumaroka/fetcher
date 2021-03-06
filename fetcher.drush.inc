<?php

/**
 * Implements hook_drush_help().
 */
function fetcher_drush_help($section) {
  switch ($section) {
    case 'meta:fetcher:title':
      return dt('Fetcher commands');
    case 'meta:fetcher:summary':
      return dt('Interact locally with sites managed by Fetcher.');
  }
}

/**
 * Implements hook_drush_command().
 */
function fetcher_drush_command() {

  $items = array();

  // Options necessary for interacting with sites on this server.
  $environment_options = array(
    'database-user-name' => 'The user name of the database user the site should use, defaults to site name.',
    'database-user-password' => 'The password of the database user the site should use.',
    'database' => 'The name of the databse the site sould use, defaults to site name.',
    'database-port' => 'The port the database runs on.',
    'database-hostname' => 'The hostname of the server the database runs on.',
    'database-admin-user-name' => 'The database username to use for administration commands.',
    'database-admin-user-password' => 'The database password to use for administration commands.',
    'site' => 'The site (folder withing the `sites` folder) on which we are acting.  Defaults to `default`.',
    'local-name' => 'The local name of the site to use for this deployment. This option allows you to get more than one variation of a site on one system.',
    'site' => 'The folder to use in drupal\'s `sites/` folder, defaults to default.',
    'hostname' => 'The hostname to use in the server config (defaults to [site_name].[hostname]).',
    'local-environment' => "The environment you are on, e.g., 'prod', 'dev'. Defaults to 'local'.",
    'remote-environment' => 'The name of the environment to load from.',
    'json-config' => 'Abitrary JSON that will be merged into the configuration of the site object.',
  );

  $items['fetcher-list'] = array(
    'description' => 'List all sites available to Fetcher that this user has access to.',
    'aliases' => array('fel'),
    'arguments' => array(
      'search' => 'Any part of the name of a site to search for.',
      'page' => 'The page of listings starting from zero.  Necessary only if there are too many results to display on a page of 100.',
    ),
  );

  $items['fetcher-info'] = array(
    'description' => 'List known information about a site managed by Fetcher.',
    'aliases' => array('fef'),
    'arguments' => array(
      'search' => 'The name of a site to list information for.',
    ),
    'options' => array(
      'fetch-info' => 'If set, do not load information about this site from the site\'s local cache, fetch it freshly from the info provider.',
      'environment' => 'The name of the environment to load from.  Optional but required with database or file syncing.',
      'json-config' => 'Abitrary JSON that will be merged into the configuration of the site object.',
    ),
  );

  // Environment items are the items that inherit the environment options.
  $environment_items['fetcher-fetch'] = array(
    'description' => 'Setup a site on this machine managed by Fetcher synchronizing the database and files if desired.',
    'aliases' => array('fetch', 'fetcher-get'),
    'arguments' => array(
      'name' => 'The name of a site to install on this server.',
    ),
    'options' => array(
      'working-directory' => 'The diretory in which to create this site instance.',
      'sql-sync' => 'Whether to syncronize the database with a selected remote environment.',
      'file-sync' => array(
        'description' => 'Whether to syncronize the `public` or `private` files with the selected remote environment. This accepts options but defaults to both.',
        'value' => 'optional',
        'example-value' => 'private',
      ),
      //'fetch-info' => 'If set, do not load information about this site from the site\'s local cache, fetch it freshly from the info provider.',
          ),
  );

  $environment_items['fetcher-create'] =  array(
    'description' => 'Setup a site on this machine similar to an Fetcher site.  Note this will not add the site to the Fetcher server.',
    'aliases' => array('fec'),
    'arguments' => array(
      'site_name' => 'The name of a site to create.',
      'version' => 'The version of Drupal to install (the same options that would be passed to pm-download without the `drupal-` prefix).',
    ),
    'options' => array(
      'package-handler' => 'The package handler to use for downloading drupal, defaults to drupalorg_git but any can be used that work with pm-download.',
      'host-name' => NULL,
      'environment-local' => "The environment you are on, e.g., 'prod', 'dev'. Defaults to 'local'.",
      'working-directory' => 'The diretory in which to create this site instance.',
      'profile' => 'The profile to download. This is passed to `drush dl` and so version numbers can be specified in the normal drush package way. Defaults to `drupal`.',
    ),
  );

  $environment_items['fetcher-delete'] = array(
    'description' => 'Remove a Fetcher managed site from this server entirely.',
    'aliases' => array('fed'),
    'arguments' => array(
      'site' => 'The name of the site to delete.',
    ),
    'options' => array(
    ),
  );

  $environment_items['fetcher-task'] = array(
    'description' => 'Perform a specific Fetcher task.',
    'aliases' => array('fet'),
    'arguments' => array(
      'task' => 'The task to perform.',
      'site' => 'The name of the site to perform the task on (defaults to the fetcher site where the command is run).',
    ),
    'options' => array(
    ),
  );

  // Add environment specific options relevant to the server
  // on which fetcher is being run for the appropriate commands.
  foreach ($environment_items as $name => $item) {
    $item['options'] = array_merge($environment_options, $item['options']);
    $items[$name] = $item;
  }

  // Add the base options for all Fetcher commands.
  foreach ($items as &$item) {
    $item['bootstrap'] = DRUSH_BOOTSTRAP_DRUSH;
  }
  return $items;
}

/**
 * List sites available with the current configuration.
 *
 * TODO: Page there should be an option for number of items to display.
 */
function drush_fetcher_list($name = '', $page = 0) {
  try {
    $result = drush_fetcher_get_info_fetcher()->listSites($name, $page);
    // TODO: We should have one common error status to check.
    if (!empty($result)) {
      $rows[] = array(
        'Title',
        'Name',
      );
      foreach($result as $name => $info) {
        $rows[] = array($info['title'], $name);
      }
      drush_print_table($rows, TRUE);
    }
  }
  catch (Exception $e) {
    drush_fetcher_log_exception_information($e);
  }
}

/**
 * Attempt to identify the site we are working with from the specified path.
 *
 * Searches out from the site_info.yaml file.
 *
 * @return
 *   A stdClass object containing site information or FALSE on failure.
 */
function fetcher_find_site_info_from_path($directory) {
  $parts = explode(DIRECTORY_SEPARATOR, $directory);
  $site_info = FALSE;
  $path = '';
  while (count($parts)) {
    $path = implode(DIRECTORY_SEPARATOR, $parts) . DIRECTORY_SEPARATOR . 'site_info.yaml';
    if (file_exists($path)) {
      return $path;
    }
    array_pop($parts);
  }
}

/**
 * Attempt to find a site info path from current direocry or site alias.
 */
function fetcher_find_site_info_path_from_name_or_cwd($name) {
  $path = FALSE;
  if (strpos($name, '@') === 0 && $specs = drush_sitealias_get_record($name)) {
    $path = $specs['root'];
  }
  if (!$name && !$path) {
    $path = getcwd();
  }
  return fetcher_find_site_info_from_path($path);
}

/**
 * Constructor function for creating and configuring a site object.
 *
 * @param $name
 *   The name of the site to load.
 * @return
 *   An instantiated and configured \Fetcher\Site object.
 */
function fetcher_get_site($name = FALSE) {

  $conf = array();

  // If we have an alias or no name at all try to find a site_info.yml file.
  if (!$name || strpos($name, '@') === 0) {
    if ($site_info_path = fetcher_find_site_info_path_from_name_or_cwd($name)) {
      $conf['site.info path'] = $site_info_path;
    }
  }

  // Set drush appropriate configurators.
  $conf['configurators'] = array(
    '\Fetcher\Configurator\DrushPrompts',
  );

  // TODO: Move this somewhere to detect configuration.
  if (is_dir('/etc') && is_file('/etc/issue')) {
    $issue = strtolower(file_get_contents('/etc/issue'));
    if (strpos($issue, 'centos') !== FALSE || strpos($issue, 'red hat') !== FALSE) {
      $conf['configurators'][] = '\Fetcher\Configurator\CentOS';
    }
  }

  $site = new \Fetcher\Site($conf);
  if ($name) {
    $site['name.global'] = $name;
  }

  // Use conifgured defaults from drushrc files.
  $site->configure(drush_fetcher_extract_keys_from_context());

  // First we apply configuration from the global context
  // because it could effect the behavior of the info fetcher.
  drush_fetcher_extract_configuration_keys_from_drush_context($site);

  // Next we fetch information specific to the site, that includes calls
  // to the infoFetcher if no site_info.yaml was already found.
  $site->fetchInfo();

  // Now we re-apply configuration keys form the context because anything
  // specified at the command line should override what was cached in the
  // site_info.yml file.
  drush_fetcher_extract_configuration_keys_from_drush_context($site);

  // Scan all user space functions to allow simple task additions.
  foreach ($site['task_loader']->scanAllUserSpaceFunctions() as $task) {
    $site->addTask($task);
  }

  // Configure from the remote environment (if remote environment is not set, and there are
  // no environments this should take no action).
  $site->configureFromEnvironment();

  // Allow other modules to modify the site object.
  drush_command_invoke_all('fetcher_site_alter', $site);

  return $site;
}

/**
 * Print out information about a single site.
 */
function drush_fetcher_info($site_name = NULL) {
  $site = fetcher_get_site($site_name);
  drush_print_r($site->exportConfiguration());
}

/**
 * Create and install a new site.
 */
function drush_fetcher_create($site_name = FALSE, $version = '7') {

  // Build a new site object.
  $site = fetcher_get_site($site_name);

  $site['version'] = $version;
  $site['code_fetcher.class'] = 'Fetcher\CodeFetcher\Download';

  try {
    drush_fetcher_ensure_site($site); // Install the site
    // TODO: Move this part into a task.

    // The profile name specified may have release info in the string (e.g.
    // bear-7.x-1.x-dev), filter that out leaving just the package name.
    $pattern = '/([a-zA-Z0-9_-])-[0-9]\.x-(.*)/';
    $profile_name = preg_replace($pattern, '$1', $site['profile']);
    $commandline_args = array(
      $profile_name,
    );
    $commandline_options = array(
      'account-name' => 'admin',
      'account-pass' => 'pass',
      'site-name' => $site['name'],
      'site' => $site['site'],
    );
    $alias = $site['site.code_directory'] . '#' . $site['site'];
    $human_readable_options = '';
    foreach ($commandline_options as $key => $value) {
      $human_readable_options .= " --$key=\"$value\"";
    }
    $human_readable_args = implode(' ', $commandline_args);
    drush_log("Executing: `drush $alias site-install $human_readable_args $human_readable_options`", 'notice');
    if (!$site['simulate']) {
      drush_invoke_process($alias, 'site-install', $commandline_args, $commandline_options);
    }
    drush_log(dt('Admin user created with username `admin` and password `pass`.'));
  }
  catch (Exception $e) {
    drush_log($e->getMessage(), 'error');
    drush_fetcher_log_exception_information($e);
    drush_log(dt('Something failed in site creation, consider running `drush fetcher-delete @site_name.', array('@site_name', $site_name)), 'error');
  }
}

/**
 * Setup a local copy of a Fetcher site.
 *
 * Do all of the work to setup a local copy of a site tracked by Fetcher,
 * optionally syncronizing the database and files with an environment
 * tracked by Fetcher.
 */
function drush_fetcher_fetch($site_name = FALSE) {

  $site = fetcher_get_site($site_name);
  $site->configureFromEnvironment();

  if (drush_get_option('sql-sync')) {
    // Add the db sync task to the stack.
    $site->addSubTask('ensure_site', 'sync_db');
  }
  // TODO: Deprecate this function all the task running function instead.
  if (!drush_fetcher_ensure_site($site)) {
    drush_set_error(dt('Provisioning and synchronizing the site has failed.'));
    return FALSE;
  }

  // TODO: This should be a task and should be as simple as adding the db
  // synchronziing task above.
  // If requested, sync the files.
  if (drush_get_option('file-sync')) {
    $files = drush_get_option('file-sync', FALSE);
    // Allow users to choose only public or only private files. Default to both.
    if ($files === TRUE) {
      $files = 'both';
    }
    else if ($files != 'public' && $files != 'private' && $files != 'both') {
      drush_log(dt('Invalid file sync flag. Files were NOT synced. Use \'public\' or \'private\'.'), 'error');
      return FALSE;
    }
    drush_log(dt('Attempting to sync files from remote...'), 'ok');
    if ($files == 'public' || $files == 'private') {
      drush_log(dt("Detected file flag. Syncing only the $files files."), 'ok');
    }
    $synced = $site->syncFiles($files);
    switch ($synced) {
      case 3:
        drush_log(dt('The files were properly synchronized.'), 'success');
        break;
      case 2:
        drush_log(dt('The private files were properly synchronized but no public files were synchronized.'), 'success');
        break;
      case 1:
        drush_log(dt('The public files were properly synchronized but no private files were synchronized.'), 'success');
        break;
      case 0:
      default:
        drush_log(dt('File sync failed. This commonly occurs when you do not have a database for your site. Try running again with --sql-sync'), 'error');
    }
  }
}

/**
 * Ensure that object is in place as it is configured.
 */
function drush_fetcher_ensure_site(Fetcher\SiteInterface $site) {
  // Note that the Fetcher library generally throws exceptions with helpful
  // error messages if anything goes wrong anywhere, so we wrap all our work
  // in one big try/catch.
  try {
    $site->runTask('ensure_site');
    return TRUE;
  }
  catch (Exception $e) {
    drush_fetcher_log_exception_information($e);
    drush_log('Something went wrong along the way, try running with `--verbose` to find the problem.', 'error');
    return FALSE;
  }
}

/**
 * Get the conifugred siteInfoFetcher().
 *
 * @return
 *   An instantiated object implmeenting \Fetcher\InfoFetcherInterface().
 */
function drush_fetcher_get_info_fetcher() {
  $config = drush_fetcher_extract_keys_from_context();
  $site = new \Fetcher\Site($config);
  return $site['info_fetcher'];
}


/**
 * Delete an fetcher site.
 */
function drush_fetcher_delete($site_name) {
  // TODO: Validate $site_name.
  // TODO: Allow deletion of other webroots (check site info?).
  if (!drush_confirm(dt("Are you sure you want to delete @site_name?", array('@site_name' => $site_name)))) {
    return;
  }
  try {
    $site = fetcher_get_site($site_name);
    $site->runTask('remove_site');
  }
  catch (Exception $e) {
    drush_fetcher_log_exception_information($e);
    drush_log($e->getMessage(), 'error');
  }
}

/**
 * List and run tasks available on a fetcher site.
 */
function drush_fetcher_task($task_name = NULL, $site_name = FALSE) {
  $site = fetcher_get_site($site_name);
  $site->configureFromEnvironment();

  if (is_null($task_name)) {
    drush_fetcher_list_tasks($site);
  }
  else {
    try {
      $site->runTask($task_name);
    }
    catch (Exception $e) {
      drush_fetcher_log_exception_information($e);
    }
  }
}

/**
 * Lists available tasks.
 */
function drush_fetcher_list_tasks($site) {
  if (!$site) {
    return drush_log('No site was found', 'error');
  }
  $rows = array();
  $rows[] = array(
    'Name',
    'Description',
  );
  $tasks = $site->getTasks();
  ksort($tasks);
  foreach ($tasks as $name => $task) {
    if (!empty($task->description)) {
      $rows[] = array($name, $task->description);
    }
  }
  drush_print_table($rows, TRUE);
}

/**
 * Configure a site from drush context and options.
 *
 * @param $site
 *   The site object, must implement \Fetcher\SiteInterface.
 */
function drush_fetcher_extract_configuration_keys_from_drush_context(\Fetcher\SiteInterface $site) {
  $keys = drush_command_invoke_all('fetcher_option_key_mapping', $site);
  drush_command_invoke_all('fetcher_option_key_mapping_alter', $keys);
  // Load any config from the global context and set them on the site object.
  if (!empty($keys['context'])) {
    foreach ($keys['context'] as $drush_name => $config_name) {
      if ($value = drush_get_context($drush_name)) {
        $site[$config_name] = $value;
      }
    }
  }
  // Load any options from the cli parameters and configure them on the site object.
  if (!empty($keys['options'])) {
    foreach ($keys['options'] as $drush_name => $config_name) {
      if ($value = drush_get_option($drush_name)) {
        $site[$config_name] = $value;
      }
    }
  }
  // We allow arbitrary injection of JSON overrides from the command line to
  // allow for overrides of items that aren't exposed as native options.
  $json_config = drush_get_option('json-config', FALSE);
  if ($json_config) {
    if ($config = json_decode($json_config)) {
      foreach($config as $key => $value) {
        $site[$key] = $value;
      }
    }
    else {
      drush_log('Invalid JSON received for json-config parameter.', 'error');
    }
  }
}

/**
 * Implements hook_fetcher_option_key_mapping().
 */
function fetcher_fetcher_option_key_mapping() {
  return array(
    'context' => array(
      'DRUSH_SIMULATE' => 'simulate',
      'DRUSH_VERBOSE' => 'verbose',
    ),
    'options' => array(
      'local-environment' => 'environment.local',
      'remote-environment' => 'environment.remote',
      'database-user-name' => 'database.user.name',
      'database-user-password' => 'database.user.password',
      'database' => 'database.database',
      'database-hostname' => 'database.hostname',
      'database-port' => 'database.port',
      'database-admin-user-name' => 'database.admin.user.name',
      'database-admin-user-password' => 'database.admin.user.password',
      'hostname' => 'hostname',
      'site' => 'site',
      'local-name' => 'name',
      'working-directory' => 'site.working_directory',
      'profile' => 'profile',
    ),
  );
}

/**
 * Get an array of fetcher sites for all environments on a host.
 *
 * This will return only sites that appear on a server with a given hostname
 * and of for those sites only environments that are on that host.
 *
 * @param $server_hostname
 *   The hostname of the sites to filter for, must be an exact match.
 */
function drush_fetcher_get_sites_for_host($server_hostname = NULL) {
  /*
  // Needs a refactor...
  $site = fetcher_get_site();
  if (is_null($server_hostname)) {
    $server_hostname = $site['system']->getHostname();
  }
  $options = array(
    'detail' => 'true',
    'server-hostname' => $server_hostname,
  );
  $site_info_list = drush_fetcher_get_info_fetcher()
    ->listSites('', 0, $options);
  $arrayify = function ($object) use (&$arrayify) {
    if (is_object($object)) {
      foreach ($object as &$item) {
        if (is_object($item)) {
          $item = $arrayify($item);
        }
      }
    }
    return (array) $object;
  };
  $sites = array();
  if (!empty($site_info_list)) {
    foreach ($site_info_list as $site_info) {
      $site_info = $arrayify($site_info);
      foreach ($site_info['environments'] as $name => $environment) {
        $local_site_info = array_merge($site_info, $environment);
        // We do this twice because otherwise we will be prompted below.
        $local_site_info['environment.local'] = $name;
        $local_site_info['environment.remote'] = $name;

        // For some reason the local site info data is being overwritten
        // getting the site and causing errors if there is more than one
        // environment, so we are 'cloning' the array before passing it.
        $get_site_info = unserialize(serialize($local_site_info));

        $site = drush_fetcher_get_site($get_site_info);
        $site['environment.local'] = $name;
        $site['environment.remote'] = $name;
        $site['site.webroot'] = $environment['root'];
        $sites[] = $site;
      }
    }
  }
  return $sites;
  */
}

/**
 * Combine all drushrc.php settings inside the fetcher namespace.
 */
function drush_fetcher_extract_keys_from_context() {
  $context = array_reverse(drush_get_context());
  $fetcher_settings = array();
  foreach ($context as $key => $value) {
    if (isset($value['fetcher']) && is_array($value['fetcher'])) {
      $fetcher_settings[] = $value['fetcher'];
    }
  };
  if (!empty($fetcher_settings)) {
    $fetcher_settings = call_user_func_array('array_merge', $fetcher_settings);
  }
  return $fetcher_settings;
}

/**
 * Implements hook_drush_init().
 *
 * Register our autoloader.
 */
function fetcher_drush_init() {
  $command = drush_get_command();
  // Check to see if we're running a Fetcher command.
  if (strpos($command['command-hook'], 'fetcher-') === 0) {
    // Include our autolaoder (allows us to autoload fetcher extension's classes).
    spl_autoload_register('fetcher_autoload');
    // Include composer's autoloader.
    require __DIR__ . '/vendor/autoload.php';
  }
}

/**
 * Autoloader for Fetcher.
 *
 * @param $class
 *   The name of the class to load.
 * @return
 *   True if the class was successfully loaded.
 */
function fetcher_autoload($class) {

  static $search_paths;

  // Only take action if searching for an Fetcher class.
  if ($search_paths === NULL) {
    $search_paths = drush_command_invoke_all('fetcher_search_paths');
    drush_command_invoke_all_ref('fetcher_search_paths_alter', $search_paths);
  }
  // Try the search paths in order.
  foreach ($search_paths as $base_path) {
    $file = $base_path . strtr($class, '\\', '/') . '.php';
    if (is_file($file)) {
      require $file;
      return TRUE;
    }
  }
}

/**
 * Implements hook_fetcher_search_paths().
 */
function fetcher_fetcher_search_paths() {
  $command = drush_get_command();
  return array(
    // Classes in base Fetcher install.
    __DIR__ . '/lib/',
  );
}

/**
 * Load an Fetcher asset and populate it with variables.
 *
 * We do not have Drupal's templating system available to us at the bootstrap
 * level where we start needing file assets so we implement our own dead
 * simple php template engine here.
 *
 * @param $asset_name
 *   The name of the file as it appears in the `/assets` directory or a fully .
 * @param $vars
 *   An array of variables for the asset, each will be available in the scope
 *   of the asset template with a variable name of the key and a value of the
 *   value.
 * TODO: This should be moved into the site object.
 */
function drush_fetcher_get_asset($asset_name, array $vars = array()) {
  $path = __DIR__ . '/assets/' . $asset_name . '.tpl.php';
  if (is_file($path)) {
    foreach ($vars as $name => $value) {
      $$name = $value;
    }
    ob_start();
    include($path);
    $contents = ob_get_contents();
    ob_end_clean();
    return $contents;
  }
  else {
    throw new \Fetcher\Exception\FetcherException(dt('The requested resource !path could not be found.', array('!path' => $path)));
  }
}

/**
 * Logs information in a standard way from an Exception that occured.
 */
function drush_fetcher_log_exception_information(Exception $e) {
  $args = array(
    '@error' => $e->getMessage(),
    '@line' => $e->getLine(),
    '@file' => $e->getFile()
  );
  drush_log(dt('@error on line @line of @file.', $args), 'error');
}


